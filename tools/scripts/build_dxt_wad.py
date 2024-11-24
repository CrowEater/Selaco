#!.venv/bin/python
import os
import sys
import shutil
import struct
import re
import zipfile
from wand.image import Image
from zipfile import ZipFile
from alive_progress import alive_bar
import Cocoa

GAME_NAME = "Selaco-EA"
GAME_INI =  "selaco-ea.ini"
GAME_WAD = 'Selaco.ipk3'

# all these images have an alpha channel, but mostly it's 1.0
def has_alpha_channel(img):
    for row in img:
        for col in row:
            if col.alpha_int8 != 255:
                return True
    return False

def copy_file(inputfile, outputfile):
    shutil.copyfile(inputfile, outputfile)
    return

# FourCC    DDS
#struct DDSURFACEDESC2
#{
#	uint32_t			Size;		// Must be 124. DevIL claims some writers set it to 'DDS ' instead.
#	uint32_t			Flags;
#	uint32_t			Height;
#	uint32_t			Width;
#	union
#	{
#		int32_t		Pitch;
#		uint32_t	LinearSize;
#	};
#	uint32_t			Depth;
#	uint32_t			MipMapCount;
#	union
#	{
#		int32_t				Offsets[11];
#		uint32_t			Reserved1[11];
#	};
#	DDPIXELFORMAT		PixelFormat;
#	DDCAPS2				Caps;
#	uint32_t			Reserved2;
#};

# these are DDS files, but they appear to have custom data in them in the "Reserved" section
# that data needs to be copied over
def convert_bc7_to_bc3(inputBytes, filename):
    Size, Flags, Height, Width = struct.unpack('iiii', inputBytes[4:20])
    MipMapCount, = struct.unpack('i', inputBytes[28:32])
    #Offsets = struct.unpack('iiiiiiiiiii', inputBytes[32:76])

    intermediateBytes = []

    with Image(blob=inputBytes) as img:
        Offset = 148  # header size 'dds ' + sizeof(DDSURFACEDESC2) + sizeof(DX10HEADER)
        MipMapIndex = 2
        BytesPerPixel = 1
        HasAlpha = has_alpha_channel(img)
        while(MipMapIndex <= MipMapCount):
            # skip parent block
            AlignedWidth = (Width + 3) & ~3
            AlignedHeight = (Height + 3) & ~3
            Offset += max(AlignedHeight * AlignedWidth * BytesPerPixel, 16)

            Width = max(1, int(Width/2))
            Height = max(1, int(Height/2))
            mipBytes = inputBytes[0:12] + struct.pack('ii', Height, Width) + inputBytes[20:148] + inputBytes[Offset:]
            with Image(blob = mipBytes) as mipimg:
                img.image_add(mipimg)
            MipMapIndex += 1

        # dxt5 is basically dxt1 with an alpha channel, if we don't need it we can get better compression by just using dxt1 
        if HasAlpha:
            img.options['dds:compression'] = 'dxt5'
        else:
            img.options['dds:compression'] = 'dxt1'
 
        img.options['dds:mipmaps'] = 'fromlist'
        intermediateBytes = img.make_blob()

    # add back the 'Offsets'
    NewMipMapCount, = struct.unpack('i', intermediateBytes[28:32])
    outputBytes = intermediateBytes[0:32] + inputBytes[32:76] + intermediateBytes[76:]
    return outputBytes

# this does roughly what's in i_specialPaths.m
def getSpecialPath(pathEnum, domainMask):
    paths = Cocoa.NSSearchPathForDirectoriesInDomains(pathEnum, domainMask, True )
    if len(paths) == 0:
        return None
    return paths[0]

# would typically return ~/Library/Application Support
def userApplicationSupportFolder():
    return getSpecialPath(Cocoa.NSApplicationSupportDirectory, Cocoa.NSUserDomainMask)

# would typically return /Library/Application Support
def localApplicationSupportFolder():
    return getSpecialPath(Cocoa.NSApplicationSupportDirectory, Cocoa.NSLocalDomainMask)

# would typically return ~/Documents
def userDocumentsDirectory():
    return getSpecialPath(Cocoa.NSDocumentDirectory, Cocoa.NSUserDomainMask)

# would typically return ~/Library/Preferences
def userPreferencesFolder():
    return os.path.join(getSpecialPath(Cocoa.NSLibraryDirectory, Cocoa.NSUserDomainMask), "Preferences")

# would typically return /Library/Preferences
def localPreferencesFolder():
    return os.path.join(getSpecialPath(Cocoa.NSLibraryDirectory, Cocoa.NSLocalDomainMask))

def getPreferencesFileName():
    # get user path, see if selaco-ea.ini exists there
    userPath = os.path.join(userPreferencesFolder(), GAME_INI)
    if os.path.exists(userPath):
        return userPath
    # if it doesn't exist get path for all users
    localPath = os.path.join(localPreferencesFolder(), GAME_INI)
    if os.path.exists(localPath):
        return localPath
    
    # if that doesn't exist, the file doesn't exist yet, user path is preferrable
    return userPath

# apparently doom uses ini files, in a non standard ini file way, so we can't just use configparser
def patchSelacoPreferences(fileName, autoLoad):
    if os.path.exists(fileName):
        # selaco-ea.ini exists we need to patch it
        lines = []
        with open(fileName) as file:
            lines = file.read().splitlines()
            
        for index, line in enumerate(lines):
            if line.strip('\n') == '[selaco.Autoload]':
                lines.insert(index+1, 'Path=' + autoLoad)
                break
    else:
        # create selaco-ea.ini, selaco should keep this as it reads it in ??
        lines = ['[selaco.Autoload]', 'Path=' + autoLoad]

    # write out new file
    with open(fileName, 'w') as file:
        file.write('\n'.join(lines) + '\n')

def getWadSearchPath(preferencesFile):
    # get our program directory
    scriptpath = sys.argv[0]
    match = re.search(r'(.*\.app)\/.*.py', scriptpath)
    if match:
        progdir = match.group(1) + '/Contents/MacOS'
    else:
        progdir = os.path.split(scriptpath)[0]

    searchpath = []

    # lets see if we can find it in .ini file, if we can read the paths
    if os.path.exists(preferencesFile):
        # selaco-ea.ini exists we need to patch it
        lines = []
        with open(preferencesFile) as file:
            lines = file.readlines()
        
        # skip all lines up to 'IWADSearch.Directories
        for index in range(0, len(lines)):
            match = re.search(r'\[(.*)\]', lines[index])
            if match and match.group(1) == 'IWADSearch.Directories':
                break

        # read all directories or break if we find a new section
        for index in range(index+1, len(lines)):
            match = re.search(r'\[(.*)\]', lines[index])
            if match:
                break
            match = re.search(r'Path=(.*)', lines[index])
            if match:
                path = match.group(1)
                if path == '$PROGDIR':
                    path = progdir
                if path == '.':
                    path = os.getcwd()
                searchpath += [path]
        return searchpath
    
    # otherwise return some sensible defaults
    home = os.path.expanduser("~")
    return [ os.getcwd(),
             os.path.join(home, 'Documents/Selaco-EA'),
             os.path.join(userApplicationSupportFolder(), GAME_NAME),
             progdir,
             os.path.join(localApplicationSupportFolder(), GAME_NAME)]

def findFileInPath(paths, fileName):
    for path in paths:
        if os.path.exists(os.path.join(path, fileName)):
            return path
    return None

def main():
    iniFilePath = os.path.join(getPreferencesFileName())
    wadSearchPaths = getWadSearchPath(iniFilePath)
    path = findFileInPath(wadSearchPaths, GAME_WAD)
    if not path:
        raise "Unable to find Selaco.ipk3"
    inputfile = os.path.join(path, GAME_WAD)
    outputfile = os.path.join(path, 'dxt.pk3')
    if not inputfile:
        print('unable to find Selaco.ipk3 in ' + wadSearchPaths)
        return
    
    with ZipFile(outputfile, 'w', zipfile.ZIP_DEFLATED, True, 5) as outputzip, ZipFile(inputfile, 'r') as inputzip:
        with alive_bar(len(inputzip.infolist())) as bar:
            for info in inputzip.infolist():
                if info.is_dir():
                    bar()
                    continue
                
                name = info.filename
                filedata = inputzip.read(name)
                if os.path.splitext(name)[1].lower() == '.dds':
                    filedata = convert_bc7_to_bc3(filedata, name)
                    # put filename in unix format (otherwise zip won't create directories inside zipfile)
                    outputzip.writestr(name.replace('\\', '/'), filedata)
                bar()
    patchSelacoPreferences(iniFilePath, outputfile)

if __name__ == "__main__":
    main()