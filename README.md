ntfsrd
======

Tool which can scan a harddisk for remains of an NTFS filesystem.


    Usage: ntfsrd [options] {dev|image} [extract list]
      -v     verbose
      -d SAVEDIR     specify where to save extracted files
      -o DISKSTART
      -l DISKSIZE
      -c CLUSSIZE    specify clustersize
      -f OFFSET      where to start searching
      -m OFFSET      specify mft offset
      -b OFFSET      specify boot offset

    the CLUSSIZE and DISKSTART are needed when you want to copy files
    they can either be obtained from the bootsector, or manually specified
    specifying DISKSIZE allows ntfsrd to look at the 2nd copy of the bootsector

Author
======

Willem Hengeveld <itsme@xs4all.nl>

