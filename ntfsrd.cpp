#include <vector>
#include <map>
#include <set>
#include <memory>
#include "util/HiresTimer.h"
#include "util/ReadWriter.h"
#include "util/rw/BlockDevice.h"
#include "util/rw/FileReader.h"
#include "util/rw/MmapReader.h"
#include "util/rw/OffsetReader.h"
#include "args.h"

// /Users/itsme/gitprj/repos/ntfsprogs-2.0.0/include/ntfs/layout.h
class ntfsdisk;
typedef std::shared_ptr<ntfsdisk> ntfsdisk_ptr;
class ntfsdisk {
    ReadWriter_ptr  _r;
    uint32_t _clustersize;
public:
// ntfsrec : type -> ntfsfile
// attr : type -> ..
    class ntfsfile {
        ntfsdisk_ptr _disk;

        uint64_t _ofs;
        uint32_t _magic;
        uint16_t _usaofs;
        uint16_t _usacount;
        uint64_t _lsn;
        uint16_t _seqnr;
        uint16_t _linkcount;
        uint16_t _attrofs;
        uint16_t _flags;
        uint32_t _bytesused;
        uint32_t _bytesalloced;
        uint64_t _basemftrecord;
        uint16_t _nextattr;
        uint16_t _reserved;
        uint32_t _mfrrecnum;

        class ntfsattr {
            ntfsdisk_ptr _disk;
            uint64_t _ofs;

            uint32_t _type;
            uint32_t _length;
            uint8_t _nonresident;
            uint8_t _namelength;
            uint16_t _nameoffset;
            uint16_t _flags;
            uint16_t _instance;

            // nonres
            uint64_t _lowvcn;
            uint64_t _highvcn;
            uint16_t _vcnmapoffset;
            uint8_t _comprunit;
            uint8_t _nrreserved[5];
            uint64_t _diskallocsize;
            uint64_t _diskdatasize;
            uint64_t _diskinitsize;

            // resident
            uint32_t _vallen;
            uint16_t _valofs;
            uint8_t _rflags;
            uint8_t _rreserved;

            uint32_t _database;
            uint32_t _datastart;
            uint32_t _datalength;
            ByteVector _data;
            std::string _name;
            std::string _filename;
            bool _islongfilename;
            std::map<uint64_t,uint64_t> _lcnmap;
        public:
            enum {
                    AT_UNUSED			= 0,
                    AT_STANDARD_INFORMATION	= 0x10,
                    AT_ATTRIBUTE_LIST		= 0x20,
                    AT_FILE_NAME		= 0x30,
                    AT_OBJECT_ID		= 0x40,
                    AT_SECURITY_DESCRIPTOR	= 0x50,
                    AT_VOLUME_NAME		= 0x60,
                    AT_VOLUME_INFORMATION	= 0x70,
                    AT_DATA			= 0x80,
                    AT_INDEX_ROOT		= 0x90,
                    AT_INDEX_ALLOCATION		= 0xa0,
                    AT_BITMAP			= 0xb0,
                    AT_REPARSE_POINT		= 0xc0,
                    AT_EA_INFORMATION		= 0xd0,
                    AT_EA			= 0xe0,
                    AT_PROPERTY_SET		= 0xf0,
                    AT_LOGGED_UTILITY_STREAM	= 0x100,
                    AT_FIRST_USER_DEFINED_ATTRIBUTE	= 0x1000,
                    AT_END			= 0xffffffff,
            };


            ntfsattr(ntfsdisk_ptr disk, uint64_t ofs, uint32_t type)
                : _disk(disk), _islongfilename(false)
            {
                _ofs= ofs;

                _type= type;
                _length= _disk->rd()->read32le();
                _nonresident= _disk->rd()->read8();
                _namelength= _disk->rd()->read8();
                _nameoffset= _disk->rd()->read16le();
                _flags= _disk->rd()->read16le();
                _instance= _disk->rd()->read16le();

                if (_nonresident) {
                    _lowvcn= _disk->rd()->read64le();
                    _highvcn= _disk->rd()->read64le();
                    _vcnmapoffset= _disk->rd()->read16le();
                    _comprunit= _disk->rd()->read8();
                    _disk->rd()->read(_nrreserved, 5);
                    _diskallocsize= _disk->rd()->read64le();
                    _diskdatasize= _disk->rd()->read64le();
                    _diskinitsize= _disk->rd()->read64le();

                    _database= 0x40;
                    _datastart= _vcnmapoffset;
                    _datalength= _length-_vcnmapoffset;
                    if (_vcnmapoffset>=_length)
                        throw "vcnmap out of range";
                }
                else {
                    _vallen= _disk->rd()->read32le();
                    _valofs= _disk->rd()->read16le();
                    _rflags= _disk->rd()->read8();
                    _rreserved= _disk->rd()->read8();

                    _database= 0x18;
                    _datastart= _valofs;
                    _datalength= _vallen;
                    if (_valofs>_length || _valofs+_vallen>_length)
                        throw "val out of range";
                }
                if (_nameoffset>_length || unsigned(_nameoffset+2*_namelength)>_length)
                    throw "name out of range";

                if (_length-_database > 512)
                    throw "attr too large";
                _data.resize(_length-_database);
                _disk->rd()->read(&_data[0], _data.size());

                for (unsigned i=0 ; i<_namelength*2 ; i+=2)
                    _name += utf8forchar(_data[i]+(_data[i+1]<<8));

                //printf("dstart=%04x, dbase= %04x, dlen=%04x, len=%04x\n", _datastart, _database, _datalength, _length);

                _data.erase(_data.begin(), _data.begin()+(_datastart-_database));
                if (_data.size()>_datalength) {
                    //printf("note: %d > %d : %d extra bytes at end of attribute\n", _data.size(), _datalength, _data.size()-_datalength);
                }
                else if (_data.size()<_datalength)
                    printf("ERROR: %d < %d : attr too short\n", (int)_data.size(), _datalength);

                if (_datalength>512)
                    throw "attr too large";
                _data.resize(_datalength);
                if (_nonresident) {
                    uint8_t *p= &_data[0];
                    uint8_t *pend= p+_data.size();
                    uint64_t  lcn= 0;
                    while (p<pend)
                    {
                        uint8_t hdr= *p++;
                        if (hdr==0)
                            break;
                        uint8_t ofssize= hdr>>4;
                        uint8_t lensize= hdr&15;

                        uint64_t len=0;
                        for (int i=0 ; i<lensize ; i++) {
                            len|=(*p++)<<(8*i);
                        }
                        uint64_t ofs=0;
                        for (int i=0 ; i<ofssize ; i++) {
                            ofs|=(*p++)<<(8*i);
                        }

                        lcn += ofs;
                        //printf("lcn: %llx, l=%x\n", lcn, len);

                        _lcnmap[lcn]= len;
                        lcn += len;
                    }
                    _data.resize(0);
                }
                else if (_type==ntfsattr::AT_FILE_NAME) {
                    // _parentdir= get32le(&_data[0])  
                    for (unsigned i=0x42 ; i<_data.size() ; i+=2)
                        _filename += utf8forchar(_data[i]+(_data[i+1]<<8));
                    _islongfilename= _data[0x41]==1;
                    //printf("fn[%d]: %s\n", _islongfilename, _filename.c_str());
                }
            }
            void copyto(ReadWriter_ptr rw)
            {
                uint64_t total= 0;
                if (_nonresident) {
                    std::for_each(_lcnmap.begin(), _lcnmap.end(), [this,rw,&total](const std::map<uint64_t,uint64_t>::value_type& kv) { 
                            _disk->rd()->setpos(kv.first*_disk->clustersize());
                            _disk->rd()->copyto(rw, std::min(_diskdatasize-total, kv.second*_disk->clustersize()));
                    });
                }
            }
            uint64_t firstcluster()
            {
                if (_lcnmap.empty())
                    return 0;
                return _lcnmap.begin()->first;
            }

            void dump()
            {
                printf("%s : %04x, n:%04x/%04x,  F:%x, I:%d, R:%d  '%-8s'", attrname().c_str(), _length, _nameoffset, _namelength, _flags, _instance, _nonresident, _name.c_str());
                if (_nonresident) {
                    printf("vcn:%llx-%llx, disk:%llx/%llx/%llx, map:%04x\n",
                            _lowvcn, _highvcn, _diskallocsize, _diskdatasize, _diskinitsize, _vcnmapoffset);

                    printf("lcnmap: ");
                    std::for_each(_lcnmap.begin(), _lcnmap.end(), [](const std::map<uint64_t,uint64_t>::value_type& kv) { printf(" %llx..%llx", kv.first, kv.first+kv.second-1); });
                    printf("\n");
                }
                else {
                    printf("V:%04x/%04x, RF:%x : '%s'\n", _valofs, _vallen, _rflags, _filename.c_str());
                }
                printf("  %04x: %s\n", _database, vhexdump(_data).c_str());
            }

            uint32_t length() const { return _length; }
            uint32_t type() const { return _type; }
            std::string filename() const { return _filename; }
            bool islongname() const { return _islongfilename; }


            std::string attrname() const
            {
                const char*names[]= {
                    "STANDARD_INFORMATION", "ATTRIBUTE_LIST", "FILE_NAME", "OBJECT_ID",
                    "SECURITY_DESCRIPTOR", "VOLUME_NAME", "VOLUME_INFORMATION", "DATA",
                    "INDEX_ROOT", "INDEX_ALLOCATION", "BITMAP", "REPARSE_POINT",
                    "EA_INFORMATION", "EA", "PROPERTY_SET", "LOGGED_UTILITY_STREAM",
                };
                if ((_type&15) || (_type>>4)<1 || (_type>>4)>16)
                    return stringformat("%04x", _type);

                return names[(_type>>4)-1];
            }
        };
        typedef std::shared_ptr<ntfsattr> ntfsattr_ptr;

        typedef std::vector<ntfsattr_ptr> ntfsattr_list;
        ntfsattr_list _attrs;

        std::string _filename;
    public:
        ntfsfile(ntfsdisk_ptr disk, uint64_t ofs)
            : _disk(disk), _ofs(ofs)
        {
            _disk->rd()->setpos(ofs);
            //    NTFS_RECORD ntfs;
            _magic= _disk->rd()->read32le();
            _usaofs= _disk->rd()->read16le();
            _usacount= _disk->rd()->read16le();

            //    MFT_RECORD  mft;
            _lsn= _disk->rd()->read64le();
            _seqnr= _disk->rd()->read16le();
            _linkcount= _disk->rd()->read16le();
            _attrofs= _disk->rd()->read16le();
            _flags= _disk->rd()->read16le();
            _bytesused= _disk->rd()->read32le();
            _bytesalloced= _disk->rd()->read32le();
            _basemftrecord= _disk->rd()->read64le();
            _nextattr= _disk->rd()->read16le();
            _reserved= _disk->rd()->read16le();
            _mfrrecnum= _disk->rd()->read32le();

            // uint16_t usa[mft.usa_count]     // ofs= ntfs.usa_ofs
            uint64_t aofs= ofs+_attrofs;
            while (true)
            {
                // ATTR_RECORD  attrs[*];          // ofs= mft.attrs_offset
     
                _disk->rd()->setpos(aofs);
                uint32_t type= _disk->rd()->read32le();
                if (type==0xFFFFFFFF)
                    break;
                try {
                _attrs.push_back(ntfsattr_ptr(new ntfsattr(_disk, aofs, type)));
                }
                catch(...)
                {
                    break;
                }

                aofs += _attrs.back()->length();
            }
        }
        void dump()
        {
            printf("%llx : LSN:%llx, bytes:%08x/%08x, base=%llx\n", _ofs, _lsn, _bytesused, _bytesalloced, _basemftrecord);
            std::for_each(_attrs.begin(), _attrs.end(), [](ntfsattr_ptr p) { p->dump(); });
        }
        void save(const std::string& savename)
        {
            ReadWriter_ptr fsave(new FileReader(savename, FileReader::createnew));

            ntfsattr_ptr dattr= find_attr_for_type(ntfsattr::AT_DATA);  // AT_VOLUME_INFORMATION ??
            if (dattr)
                dattr->copyto(fsave);
        }
        uint64_t firstcluster()
        {
            ntfsattr_ptr dattr= find_attr_for_type(ntfsattr::AT_DATA);  // AT_VOLUME_INFORMATION ??
            if (dattr)
                return dattr->firstcluster();
            return 0;
        }
        ntfsattr_ptr find_attr_for_type(uint32_t type)
        {
            auto i= std::find_if(_attrs.begin(), _attrs.end(), [type](ntfsattr_ptr p) { return p->type()==type; });
            if (i==_attrs.end())
                return ntfsattr_ptr();
            return *i;
        }
        template<typename F>
        ntfsattr_ptr find_attr(F f)
        {
            auto i= std::find_if(_attrs.begin(), _attrs.end(), f);
            if (i==_attrs.end())
                return ntfsattr_ptr();
            return *i;
        }

        std::string filename()
        {
            if (!_filename.empty())
                return _filename;

            ntfsattr_ptr nlong= find_attr( [](ntfsattr_ptr p) { return p->type()==ntfsattr::AT_FILE_NAME && p->islongname(); } );
            ntfsattr_ptr ndos= find_attr( [](ntfsattr_ptr p) { return p->type()==ntfsattr::AT_FILE_NAME && !p->islongname(); } );
            if (nlong) return _filename= nlong->filename();
            if (ndos) return _filename= ndos->filename();

            //printf("Warning: file has no name\n");
            return _filename= " ";
        }
    };


    ntfsdisk(ReadWriter_ptr r)
        : _r(r), _clustersize(0)
    {
    }
    void setclustersize(uint32_t clussize)
    {
        if (_clustersize && _clustersize!=clussize)
            printf("WARNING clustersize changed from 0x%x to 0x%x\n", _clustersize, clussize);
        _clustersize= clussize;
    }

    ReadWriter_ptr rd() const { return _r; }
    uint32_t clustersize() const { return _clustersize; }
};
class ntfsboot {
    ReadWriter_ptr _r;
    uint64_t _ofs;

    uint32_t _clustersize;
    uint64_t _nsectors;
    uint64_t _mftlcn;
    uint64_t _mirrmftlcn;
    public:
        ntfsboot(ReadWriter_ptr r, uint64_t ofs)
            : _r(r), _ofs(ofs)
        {
            if (!readheader())
                throw "invalid bootsector";
        }
    bool readheader()
    {
        _r->setpos(_ofs+0x03);
        std::string magic(8, 0);
        _r->read((uint8_t*)&magic[0], 8);
        if (magic!="NTFS    ")
            return false;
        uint16_t bytespersector= _r->read16le();
        uint8_t sectorspercluster= _r->read8();

        _clustersize= bytespersector*sectorspercluster;

        _r->setpos(_ofs+0x28);
        _nsectors= _r->read64le();
        _mftlcn= _r->read64le();
        _mirrmftlcn= _r->read64le();

        return true;
    }
    uint32_t clustersize() const { return _clustersize; };
    uint64_t nsectors() const { return _nsectors; };
    uint64_t mftclus() const { return _mftlcn; };
    uint64_t mirclus() const { return _mirrmftlcn; };
};

void usage()
{
    fprintf(stderr, "Usage: ntfsrd [options] {dev|image} [extract list]\n");
    fprintf(stderr, "  -v     verbose\n");
    fprintf(stderr, "  -d SAVEDIR     specify where to save extracted files\n");
    fprintf(stderr, "  -o DISKSTART\n");
    fprintf(stderr, "  -l DISKSIZE\n");
    fprintf(stderr, "  -c CLUSSIZE    specify clustersize\n");
    fprintf(stderr, "  -f OFFSET      where to start searching\n");
    fprintf(stderr, "  -m OFFSET      specify mft offset\n");
    fprintf(stderr, "  -b OFFSET      specify boot offset\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "the CLUSSIZE and DISKSTART are needed when you want to copy files\n");
    fprintf(stderr, "they can either be obtained from the bootsector, or manually specified\n");
    fprintf(stderr, "specifying DISKSIZE allows ntfsrd to look at the 2nd copy of the bootsector\n");
}
template<typename V, typename P>
bool all(const typename V::value_type& x, V v, P pred)
{
    return v.end()==std::find_if(v.begin(), v.end(), [pred, &x](const typename V::value_type& y) { return !pred(x,y); });
}
template<typename V, typename P>
bool all(V v, const typename V::value_type& x, P pred)
{
    return v.end()==std::find_if(v.begin(), v.end(), [pred, &x](const typename V::value_type& y) { return !pred(y,x); });
}
template<typename V, typename P>
bool any(const typename V::value_type& x, V v, P pred)
{
    return v.end()!=std::find_if(v.begin(), v.end(), [pred, &x](const typename V::value_type& y) { return pred(x,y); });
}
int main(int argc,char**argv)
{
    bool verbose= false;
    std::string devname;
    std::string savedir;
    uint64_t diskstart=0;    bool diskstartspecified = false;
    uint64_t disksize=0;
    uint64_t fileentofs= 0;  bool filentspecified = false;
    uint32_t clustersize= 0;
    uint64_t mtfent_offset = 0;
    std::set<std::string> files;

    std::vector<uint64_t> mftentofs;
    std::vector<uint64_t> bootofs;


    for (int i=1 ; i<argc ; i++)
    {
        if (argv[i][0]=='-') switch(argv[i][1])
        {
            case 'v': verbose=true; break;
            case 'd': savedir = getstrarg(argv, i, argc); break;
            case 'o': diskstart = getintarg(argv, i, argc); break;
            case 'l': disksize = getintarg(argv, i, argc); break;
            case 'c': clustersize = getintarg(argv, i, argc); break;
            case 'f': fileentofs = getintarg(argv, i, argc); filentspecified = true; break;
            case 'm': mtfent_offset = getintarg(argv, i, argc); break;
            case 'b': bootofs.push_back( getintarg(argv, i, argc) ); break;
            default:
                      usage();
                      return 1;
        }
        else if (devname.empty())
            devname= argv[i];
        else
            files.insert(argv[i]);
    }
    if (savedir.empty())
        savedir= "./";
    else if (savedir[savedir.size()-1]!='/')
        savedir += '/';

    try {
    ReadWriter_ptr f;
    if (FileReader::isblockdev(devname)) {
        f.reset(new BlockDevice(devname, BlockDevice::readonly));
        printf("blockdev reader\n");
    }
    else {
        f.reset(new MmapReader(devname, MmapReader::readonly));
        printf("mmap reader\n");
    }
    if (diskstartspecified || disksize) {
        if (disksize==0)
            disksize= f->size()-diskstart;
        f.reset(new OffsetReader(f, diskstart, disksize));
        printf("restricted to %08llx - %08llx\n", diskstart, disksize);
    }

    ntfsdisk_ptr disk(new ntfsdisk(f));
    if (clustersize) 
        disk->setclustersize(clustersize);

    auto mksetter= [](uint64_t& val, const char*msg) {
        return [&val, msg](uint64_t newval)
        {
            if (val && val!=newval)
                printf("multiple %s: 0x%llx != 0x%llx\n", msg, val, newval);
            val= newval;
        };
    };


    uint64_t mftclus= 0;
    auto setmftclus= mksetter(mftclus, "MFT entries");

    uint64_t mirclus= 0;
    auto setmirclus= mksetter(mirclus, "MFTMirr entries");

    uint64_t dsksize= 0;
    auto setdsksize= mksetter(dsksize, "dsksize values");
 
    if (mtfent_offset)
        mftentofs.push_back(mtfent_offset);
    HiresTimer t;
    for (uint64_t ofs= fileentofs ; ofs < (filentspecified ? (fileentofs+0x200) : f->size()) ; ofs+=0x200)
    {
        f->setpos(ofs);
        try {
        uint32_t magic= f->read32le();
        if (magic==0x454c4946) {            // $MFT/$DATA: Mft entry - magic_FILE
            ntfsdisk::ntfsfile nf(disk, ofs);

            if (verbose)
                nf.dump();
            if (!files.empty() && files.end()!=files.find(nf.filename())) {
                if (disk->clustersize())
                    nf.save(savedir + nf.filename());
                else {
                    printf("can't save files when clustersize is unknown\n");
                    return 1;
                }
            }

            if (nf.filename()=="$MFT") {
                setmftclus(nf.firstcluster());
                mftentofs.push_back(ofs);
            }
            else if (nf.filename()=="$MFTMirr") {
                setmirclus(nf.firstcluster());
            }
        }
        else if (magic==0x4e9052eb) {    // magic for bootsector
            ntfsboot boot(f, ofs);

            disk->setclustersize(boot.clustersize());
            setmftclus(boot.mftclus());
            setmirclus(boot.mirclus());
            setdsksize(boot.nsectors());
            bootofs.push_back(ofs);
        }
        }
        catch(const std::exception& e) {
            printf("ERR reading %08llx: %s\n", ofs, e.what());
        }
        catch(const char*msg) {
            printf("ERR reading %08llx: %s\n", ofs, msg);
        }
        catch(...) {
            printf("ERR reading %08llx\n", ofs);
        }
        if ((ofs&0xFFFFFFF)==0) {
            fprintf(stderr, "%12llx  %9.0f bytes/sec      \r", ofs, double(1000000.0*0x10000000)/t.lap());
        }
    }
    printf("FOUND: mft=0x%llx, mir=0x%llx dsk=0x%llx  clus=0x%x\n", mftclus, mirclus, dsksize, disk->clustersize());

    printf("f->size=%llx\n", f->size());

    // todo: verify these:
    //
    // 2 bootofs :   back-front == dsksize
    for (unsigned i=0 ; i<bootofs.size() ; i++)
        printf("boot%d: %llx\n", i, bootofs[i]);

    uint64_t firstboot;
    uint64_t secondboot;
    if (bootofs.size()==2) {
        if (!(all(bootofs[0], mftentofs, std::less<uint64_t>()) && all(mftentofs, bootofs[1], std::less<uint64_t>())))
        {
            printf("not all mftent between bootsectors!!! -> reduce disksize  %d %d\n", 
                    all(bootofs[0], mftentofs, std::less<uint64_t>()),
                    all(mftentofs, bootofs[1], std::less<uint64_t>()));
            printf("boot=0x%llx .. 0x%llx, mft:", bootofs[0], bootofs[1]);
            for (unsigned i=0 ; i<mftentofs.size() ; i++)
                printf(" 0x%llx", mftentofs[i]);
            printf("\n");

            return 1;
        }
        firstboot= bootofs[0];
        secondboot= bootofs[1];

        if (secondboot-firstboot!=dsksize*0x200)
            printf("boot -> dsksize= %llx,  boot: %llx\n", secondboot-firstboot, dsksize);
    }
    else if (bootofs.size()==1) {
        if (all(bootofs[0], mftentofs, std::less<uint64_t>())) {
            firstboot= bootofs[0];
            secondboot= bootofs[0]+dsksize*0x200;
            if (!all(mftentofs, secondboot, std::less<uint64_t>())) {
                printf("entries after inferred secondboot (0x%llx)\n", secondboot);
                printf("multiple volumne -> specify better disksize\n");
                return 1;
            }

        }
        else if (all(mftentofs, bootofs[0], std::less<uint64_t>())) {
            if (bootofs[0]<dsksize*0x200) {
                printf("inferred firstboot before diskstart -> specify smaller diskstart\n");
                return 1;
            }
            firstboot= bootofs[0]-dsksize*0x200;
            secondboot= bootofs[0];
            if (!all(firstboot, mftentofs, std::less<uint64_t>())) {
                printf("entries before inferred firstboot (0x%llx)\n", firstboot);
                printf("multiple volumne -> specify better diskstart offset\n");
                return 1;
            }
        }
        else {
            printf("entries before, and after bootsector -> multiple volumes\n");
            printf("-> reduce disksize\n");
            return 1;
        }
    }
    else {
        printf("no ntfsboot found: must specify clustersize and diskstart when you want to extract files\n");
        firstboot= 0;
        secondboot= 0;
    }


    struct scaninfo {
        scaninfo() :ix(0), off(0), cs(0), mir(false) { }
        scaninfo(int ix, uint64_t off, uint32_t cs, bool mir ) : ix(ix), off(off), cs(cs), mir(mir) { }
        int ix;
        uint64_t off;
        uint32_t cs;
        bool mir;
    };
    typedef std::map<uint64_t,std::vector<scaninfo> > dskstartcalcmap_t;
    dskstartcalcmap_t  dskstartcalc;
    // mftentofs :   ofs=clussize*mftclus+diskstart
    //
    for (unsigned i=0 ; i<mftentofs.size() ; i++)
    {
        for (uint32_t cs= disk->clustersize()? disk->clustersize() : 0x200 ; cs<=(disk->clustersize() ? disk->clustersize() : 0x10000) ; cs*=2)
        {
            uint64_t dsk_mft= mftentofs[i]-mftclus*cs;
            uint64_t dsk_mir= mftentofs[i]-mirclus*cs;

            //printf("cs%08x: %08llx-> mft->%llx, mir->%llx\n", cs, mftentofs[i], dsk_mft, dsk_mir);

            dskstartcalc[dsk_mir].push_back(scaninfo(i, mftentofs[i], cs, true));
            dskstartcalc[dsk_mft].push_back(scaninfo(i, mftentofs[i], cs, false));
        }
    }
    std::for_each(dskstartcalc.begin(), dskstartcalc.end(), [](const dskstartcalcmap_t::value_type& vt) { 
        if (vt.second.size()==2 && vt.second.front().cs==vt.second.back().cs)
        {
            printf("possible diskstart: 0x%llx\n", vt.first);
        }
    });
    
    }
    catch(const char*msg)
    {
        printf("E: %s\n", msg);
    }
    catch(const std::string& msg)
    {
        printf("E: %s\n", msg.c_str());
    }
    catch(...)
    {
        printf("E!\n");
    }
    return 0;
}
