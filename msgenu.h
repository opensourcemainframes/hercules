/*----------------------------------------------------------------------------*/
/* (c) Copyright Roger Bowler and others 1999-2010                            */
/* Hercules-390 mainframe emulator                                            */
/*                                                                            */
/* file: msgenu.h                                                             */
/*                                                                            */
/* (c) Copyright Bernard van der Helm 2010                                    */
/* Main header file for Hercules messages.                                    */
/*----------------------------------------------------------------------------*/

#define MSG(id, ...)      #id " " id "\n", ## __VA_ARGS__
#define WRITEMSG(id, ...) logmsg(#id " " id "\n", ## __VA_ARGS__)
//#define WRITEMSG(id, ...) logmsg("%-10s(%5d) " #id " " id "\n", __FILE__, __LINE__, ## __VA_ARGS__)

/* awstape.c */
#define HHCTA101I "Device(%4.4X): AWS Tape file(%s) closed"
#define HHCTA102E "Device(%4.4X): Error opening file(%s): (%s)"
#define HHCTA103E "Device(%4.4X): Error seeking to offset("I64_FMTX") in file(%s): (%s)"
#define HHCTA104E "Device(%4.4X): Error reading block header at offset("I64_FMTX") in file(%s): (%s)"
#define HHCTA105E "Device(%4.4X): End of file (end of tape) at offset("I64_FMTX") in file(%s)"
#define HHCTA106E "Device(%4.4X): Unexpected end of file in block header at offset("I64_FMTX") in file(%s)"
#define HHCTA107E "Device(%4.4X): Block length(%d) exceeds at offset("I64_FMTX") in file(%s)"
#define HHCTA108E "Device(%4.4X): Invalid tapemark at offset("I64_FMTX") in file(%s)"
#define HHCTA109E "Device(%4.4X): Error reading data block at offset("I64_FMTX") in file(%s): (%s)"
#define HHCTA110E "Device(%4.4X): Unexpected end of file in data block at offset("I64_FMTX") in file(%s)"
#define HHCTA111E "Device(%4.4X): Error seeking to offset("I64_FMTX") in file(%s): (%s)"
#define HHCTA112E "Device(%4.4X): Media full condition reached at offset("I64_FMTX") in file(%s)"
#define HHCTA113E "Device(%4.4X): Error writing block header at offset("I64_FMTX") in file(%s): (%s)"
#define HHCTA114E "Device(%4.4X): Media full condition reached at offset("I64_FMTX") in file(%s)"
#define HHCTA115E "Device(%4.4X): Error writing data block at offset("I64_FMTX") in file(%s): (%s)"
#define HHCTA116E "Device(%4.4X): Error writing data block at offset("I64_FMTX") in file(%s): (%s)"
#define HHCTA117E "Device(%4.4X): Error seeking to offset("I64_FMTX") in file(%s): (%s)"
#define HHCTA118E "Device(%4.4X): Error writing block header at offset("I64_FMTX") in file (%s): (%s)"
#define HHCTA119E "Device(%4.4X): Error writing tape mark at offset("I64_FMTX") in file(%s): (%s)"
#define HHCTA120E "Device(%4.4X): Sync error on file(%s): (%s)"

/* bldcfg.c */
#define HHCCF001S "Error reading file(%s) line(%d): (%s)"
#define HHCCF002S "File(%s) line(%d) is too long"
#define HHCCF003S "Open error file(%s): (%s)"
#define HHCCF004S "No device records in file(%s)"
#define HHCCF008E "Error in file(%s) line(%d): Syntax error:(%s)"
#define HHCCF009E "Error in file(%s) line(%d): Incorrect number of operands"
#define HHCCF012S "Error in file(%s) line(%d): (%s) is not a valid CPU (%s)"
#define HHCCF013S "Error in file(%s) line(%d): Invalid main storage size (%s)"
#define HHCCF014S "Error in file(%s) line(%d): Invalid expanded storage size (%s)"
#define HHCCF016S "Error in file(%s) line(%d): Invalid (%s) thread priority (%s)"
#define HHCCF017W "Hercules is not running as setuid root, cannot raise (%s) thread priority"
#define HHCCF018S "Error in file(%s) line(%d): Invalid number of CPUs (%s)"
#define HHCCF019S "Error in file(%s) line(%d): Invalid number of VFs (%s)"
#define HHCCF020W "Vector Facility support not configured"
#define HHCCF022S "Error in file(%s) line(%d): (%s) is not a valid system epoch. The only valid values are 1801-2099"
#define HHCCF023S "Error in file(%s) line(%d): (%s) is not a valid timezone offset"
#define HHCCF029S "Error in file(%s) line(%d): Invalid SHRDPORT port number (%s)"
#define HHCCF031S "Cannot obtain (%d)MB main storage: (%s)"
#define HHCCF032S "Cannot obtain storage key array: (%s)"
#define HHCCF033S "Cannot obtain (%d)MB expanded storage: (%s)"
#define HHCCF034W "Expanded storage support not installed"
#define HHCCF035S "Error in file(%s) line(%d): Missing device number or device type"
#define HHCCF036S "Error in file(%s) line(%d): (%s) is not a valid device number(s) specification"
#define HHCCF051S "Error in file(%s) line(%d): (%s) is not a valid serial number"
#define HHCCF052W "Warning in file(%s) line(%d): Invalid ECPSVM level value: (%s), 20 Assumed"
#define HHCCF053W "Error in file(%s) line(%d): Invalid ECPSVM keyword: (%s), NO Assumed"
#define HHCCF061W "Warning in file(%s) line(%d): (%s) statement deprecated. Use (%s) instead"
#define HHCCF062W "Warning in file(%s) line(%d): Missing ECPSVM level value, 20 Assumed"
#define HHCCF063W "Warning in file(%s) line(%d): Specifying ECPSVM level directly is deprecated. Use the 'LEVEL' keyword instead"
#define HHCCF064W "Hercules set priority (%d) failed: (%s)"
#define HHCCF065I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCCF070S "Error in file(%s) line(%d): (%s) is not a valid year offset"
#define HHCCF072W "SYSEPOCH (%04d) is deprecated, please specify \"SYSEPOCH 1900 (%s%d)\""
#define HHCCF073W "SYSEPOCH (%04d) is deprecated, please specify \"SYSEPOCH 1960 (%s%d)\""
#define HHCCF074S "Error in file(%s) line(%d): Invalid engine syntax (%s)"
#define HHCCF075S "Error in file(%s) line(%d): Invalid engine type (%s)"
#define HHCCF077I "Engine (%d) set to type (%d) (%s)"
#define HHCCF081I "File(%s) will ignore include errors"
#define HHCCF082S "Error in file(%s) line(%d): Maximum nesting level (%d) reached"
#define HHCCF083I "File(%s) including(%s) at (%d)"
#define HHCCF084W "File(%s) open error ignored file(%s): (%s)"
#define HHCCF085S "File(%s) open error file(%s): (%s)"
#define HHCCF086S "Error in file(%s): NUMCPU(%d) must not exceed MAXCPU(%d)"
#define HHCCF090I "Default Allowed AUTOMOUNT directory = (%s)"
#define HHCCF900S "Out of memory"

/* cache.c */
#define HHCCH001E "Calloc failed cache[%d] size(%d): (%s)"
#define HHCCH002W "Realloc increase failed cache[%d] size(%d): (%s)"
#define HHCCH003W "Realloc decrease failed cache[%d] size(%d): (%s)"
#define HHCCH004W "Buf calloc failed cache[%d] size(%d): (%s)"
#define HHCCH005W "Releasing inactive buffer space"
#define HHCCH006E "Unable to calloc buf cache[%d] size(%d): (%s)"

/* cardpch.c */
#define HHCPU004E "Error writing to file(%s): (%s)"
#define HHCPU001E "File name missing or invalid"
#define HHCPU002E "Invalid argument: (%s)"
#define HHCPU003E "Error opening file(%s): (%s)"

/* cardrdr.c */
#define HHCRD001E "Out of memory"
#define HHCRD002E "File name(%s) too long, max is (%ud)"
#define HHCRD003E "Unable to access file(%s): (%s)"
#define HHCRD004E "Out of memory"
#define HHCRD005E "Specify 'ascii' or 'ebcdic' (or neither) but not both"
#define HHCRD006E "Only one filename (sock_spec) allowed for socket devices"
#define HHCRD007I "Defaulting to 'ascii' for socket device(%4.4X)"
#define HHCRD008W "'multifile' option ignored: only one file specified"
#define HHCRD009E "File name(%s) too long, max is (%ud)"
#define HHCRD010E "Unable to access file(%s): (%s)"
#define HHCRD011E "Close error on file(%s): (%s)"
#define HHCRD012I "%s (%s) disconnected from device(%4.4X) (%s)"
#define HHCRD013E "Error opening file(%s): (%s)"
#define HHCRD014E "Error reading file(%s): (%s)"
#define HHCRD015E "Seek error in file(%s): (%s)"
#define HHCRD016E "Error reading file(%s): (%s)"
#define HHCRD017E "Unexpected end of file(%s)"
#define HHCRD018E "Error reading file(%s): (%s)"
#define HHCRD019E "Card image exceeds (%d) bytes in file(%s)"

/* cckddasd.c */
#define HHCCD001I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCCD002I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCCD003I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCCD011I "Thread ended tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCCD012I "Thread ended tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCCD013I "Thread ended tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCCD092I "%d devices processed"
#define HHCCD101E "Device(%4.4X) error initializing shadow files"
#define HHCCD102E "Device(%4.4X) file[%d] get space error, size exceeds %lldM"
#define HHCCD110E "Device(%4.4X) file[%d] devhdr id error"
#define HHCCD121E "Device(%4.4X) file[%d] trklen err for %2.2x%2.2x%2.2x%2.2x%2.2x"
#define HHCCD122E "Device(%4.4X) file[%d] invalid byte 0 trk(%d) buf(%2.2x%2.2x%2.2x%2.2x%2.2x)"
#define HHCCD123E "Device(%4.4X) file[%d] invalid byte 0 blkgrp(%d) buf(%2.2x%2.2x%2.2x%2.2x%2.2x)"
#define HHCCD124E "Device(%4.4X) file[%d] invalid %s hdr %s %d: %s compression unsupported"
#define HHCCD125E "Device(%4.4X) file[%d] invalid %s hdr %s %d buf(%p:%2.2x%2.2x%2.2x%2.2x%2.2x)"
#define HHCCD130E "Device(%4.4X) file[%d] %s open error: %s"
#define HHCCD131E "Device(%4.4X) file[%d] close error: %s"
#define HHCCD132E "Device(%4.4X) file[%d] lseek error, offset 0x%" I64_FMT "x: %s"
#define HHCCD133E "Device(%4.4X) file[%d] read error, offset 0x%" I64_FMT "x: %s"
#define HHCCD134E "Device(%4.4X) file[%d] read incomplete, offset 0x%" I64_FMT "x: read %d expected %d"
#define HHCCD135E "Device(%4.4X) file[%d] lseek error, offset 0x%" I64_FMT "x: %s"
#define HHCCD136E "Device(%4.4X) file[%d] write error, offset 0x%" I64_FMT "x: %s"
#define HHCCD137E "Device(%4.4X) file[%d] write incomplete, offset 0x%" I64_FMT "x: wrote %d expected %d"
#define HHCCD138E "Device(%4.4X) file[%d] ftruncate error, offset 0x%" I64_FMT "x: %s"
#define HHCCD139E "Device(%4.4X) malloc error, size(%d) %s"
#define HHCCD140E "Device(%4.4X) calloc error, size(%d) %s"
#define HHCCD142E "Device(%4.4X) file[%d] shadow file name(%s) collides with %4.4X file[%d] name(%s)"
#define HHCCD151E "Device(%4.4X) file[%d] error re-opening %s readonly: %s"
#define HHCCD160E "Device(%4.4X) not a cckd device"
#define HHCCD161E "Device(%4.4X) file[%d] no shadow file name"
#define HHCCD162E "Device(%4.4X) file[%d] max shadow files exceeded"
#define HHCCD163E "Device(%4.4X) file[%d] error adding shadow file"
#define HHCCD164I "Device(%4.4X) file[%d] %s added"
#define HHCCD165W "Device(%4.4X) error adding shadow file, sf command busy on device"
#define HHCCD170E "Device(%4.4X) not a cckd device"
#define HHCCD171E "Device(%4.4X) file[%d] cannot remove base file"
#define HHCCD172E "Device(%4.4X) file[%d] not merged, file[%d] cannot be opened read-write%s"
#define HHCCD173E "Device(%4.4X) file[%d] not merged, file[%d] check failed"
#define HHCCD174E "Device(%4.4X) file[%d] not merged, file[%d] not hardened"
#define HHCCD175W "Device(%4.4X) file[%d] merge failed, sf command busy on device"
#define HHCCD179I "Merging device(%d:%4.4X)"
#define HHCCD180E "Device(%4.4X) file[%d] not merged, error during merge"
#define HHCCD181I "Device(%4.4X) shadow file [%d] successfully %s"
#define HHCCD182E "Device(%4.4X) file[%d] not merged, error processing trk(%d)"
#define HHCCD190E "Device(%4.4X) file[%d] offset 0x%" I64_FMT "x unknown space(%2.2x%2.2x%2.2x%2.2x%2.2x)"
#define HHCCD193E "Device(%4.4X) file[%d] uncompress error trk(%d) %2.2x%2.2x%2.2x%2.2x%2.2x"
#define HHCCD194E "Device(%4.4X) file[%d] %s compression not supported"
#define HHCCD200I "Compressing device(%d:%4.4X)"
#define HHCCD201I "Checking device(%d:%4.4X) level(%d)"
#define HHCCD202W "Device(%4.4X) file[%d] check failed, sf command busy on device"
#define HHCCD205W "Device(%4.4X) is not a cckd device"
#define HHCCD206W "Device(%4.4X) file[%d] compress failed, sf command busy on device"
#define HHCCD207I "Adding device(%d:%4.4X)"
#define HHCCD208I "Displaying device(%d:%4.4X)"
#define HHCCD209W "Device(%4.4X) is not a cckd device"
#define HHCCD210I "          size free  nbr st   reads  writes l2reads    hits switches"
#define HHCCD211I "                                                 readaheads   misses"
#define HHCCD212I "--------------------------------------------------------------------"
#define HHCCD213I "[*] %10" I64_FMT "d %3" I64_FMT "d%% %4d    %7d %7d %7d %7d  %7d"
#define HHCCD214I "                                                    %7d  %7d"
#define HHCCD215I "%s"
#define HHCCD216I "[0] %10" I64_FMT "d %3" I64_FMT "d%% %4d %s %7d %7d %7d"
#define HHCCD217I "%s"
#define HHCCD218I "[%d] %10" I64_FMT "d %3" I64_FMT "d%% %4d %s %7d %7d %7d"
#define HHCCD900I "print_itrace"

/* channel.c */
#define HHCCP048I "Device(%4.4X) CCW(%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X%s)"
#define HHCCP049I "Device(%4.4X) Stat(%2.2X%2.2X) Count(%2.2X%2.2X) CCW(%2.2X%2.2X%2.2X)"
#define HHCCP050I "Device(%4.4X) SCSW(%2.2X%2.2X%2.2X%2.2X) Stat(%2.2X%2.2X) Count(%2.2X%2.2X) CCW(%2.2X%2.2X%2.2X%2.2X)"
#define HHCCP051I "Device(%4.4X) Test I/O"
#define HHCCP052I "TIO modification executed CC=1"
#define HHCCP053I "Device(%4.4X) Halt I/O"
#define HHCCP054I "HIO modification executed CC=1"
#define HHCCP055I "Device(%4.4X) Clear subchannel"
#define HHCCP056I "Device(%4.4X) Halt subchannel"
#define HHCCP057I "Device(%4.4X) Halt subchannel: cc=%d"
#define HHCCP060I "Device(%4.4X) Resume subchannel: cc=%d"
#define HHCCP078I "Device(%4.4X) MIDAW(%2.2X %4.4"I16_FMT"X %16.16"I64_FMT"X): %s"
#define HHCCP063I "Device(%4.4X) IDAW(%8.8"I32_FMT"X) Len(%3.3"I16_FMT"X): %s"
#define HHCCP064I "Device(%4.4X) IDAW(%16.16"I64_FMT"X) Len(%4.4"I16_FMT"X): %s"
#define HHCCP065I "Device(%4.4X) attention signalled"
#define HHCCP066I "Device(%4.4X) attention"
#define HHCCP067E "Device(%4.4X) create_thread error: %s"
#define HHCCP069I "Device(%4.4X) initial status interrupt"
#define HHCCP070I "Device(%4.4X) attention completed"
#define HHCCP071I "Device(%4.4X) clear completed"
#define HHCCP072I "Device(%4.4X) halt completed"
#define HHCCP073I "Device(%4.4X) suspended"
#define HHCCP074I "Device(%4.4X) resumed"
#define HHCCP075I "Device(%4.4X) Stat(%2.2X%2.2X) Count(%4.4X) %s"
#define HHCCP076I "Device(%4.4X) Sense(%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X)"
#define HHCCP077I "Device(%4.4X) Sense(%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s)"

/* ckddasd.c */
#define HHCDA001E "File name missing or invalid"
#define HHCDA002E "Device(%4.4X) File(%s) not found or invalid"
#define HHCDA003E "Parameter(%s) number(%d) is invalid"
#define HHCDA004I "Opening file(%s) readonly%s"
#define HHCDA005E "File(%s) open error(%s)"
#define HHCDA006E "File(%s) not in a single file for shadowing"
#define HHCDA007E "File(%s) fstat error(%s)"
#define HHCDA008E "File(%s) read error(%s)"
#define HHCDA009E "File(%s) CKD header incomplete"
#define HHCDA010E "File(%s) CKD header invalid"
#define HHCDA011E "File(%s) Only 1 CCKD file allowed"
#define HHCDA012E "File(%s) read error(%s)"
#define HHCDA013E "File(%s) CCKD header incomplete"
#define HHCDA014E "File(%s) CKD file out of sequence"
#define HHCDA015I "File(%s) seq(%d) cyls(%d-%d)"
#define HHCDA016E "File(%s) heads(%d) trklen(%d), expected heads(%d) trklen(%d)"
#define HHCDA017E "File(%s) CKD header inconsistent with file size"
#define HHCDA018E "File(%s) CKD header high cylinder incorrect"
#define HHCDA019E "File(%s) exceeds maximum CKD files(%d)"
#define HHCDA020I "File(%s) cyls(%d) heads(%d) tracks(%d) trklen(%d)"
#define HHCDA021E "Device(%4.4X) device type(%4.4X) not found in dasd table"
#define HHCDA022E "Device(%4.4X) control unit(%s) not found in dasd table"
#define HHCDA023I "Device(%4.4X) cache hits(%d), misses(%d), waits(%d)"
#define HHCDA024I "Read trk(%d) cur trk(%d)"
#define HHCDA025I "Read track: updating track(%d)"
#define HHCDA026E "Error writing trk(%d): lseek error(%s)"
#define HHCDA027E "Error writing trk(%d): write error(%s)"
#define HHCDA028I "Read trk(%d) cache hit, using cache[%d]"
#define HHCDA029I "Read trk(%d) no available cache entry, waiting"
#define HHCDA030I "Read trk(%d) cache miss, using cache[%d]"
#define HHCDA031I "Read trk(%d) reading file(%d) offset(%" I64_FMT "d) len(%d)"
#define HHCDA032E "Error reading trk(%d): lseek error(%s)"
#define HHCDA033E "Error reading trk(%d): read error(%s)"
#define HHCDA034I "Read trk(%d) trkhdr(%2.2x %2.2x%2.2x %2.2x%2.2x)"
#define HHCDA035E "Device(%4.4X) invalid track header for cyl(%d) head(%d) %2.2x%2.2x%2.2x%2.2x%2.2x"
#define HHCDA038I "Seeking to cyl(%d) head(%d)"
#define HHCDA039E "MT advance error: locate record(%d) file mask(%2.2X)"
#define HHCDA040I "MT advance to cyl(%d) head(%d)"
#define HHCDA041I "Read count orientation(%s)"
#define HHCDA042E "Attempt to read past end of track(%d %d)"
#define HHCDA043I "Cyl(%d) head(%d) record(%d) kl(%d) dl(%d) of(%d)"
#define HHCDA044I "Read key %d bytes"
#define HHCDA045I "Read data %d bytes"
#define HHCDA046E "Attempt to read past end of track"
#define HHCDA047I "Writing cyl(%d) head(%d) record(%d) kl(%d) dl(%d)"
#define HHCDA048I "Setting track overflow flag for cyl(%d) head(%d) record(%d)"
#define HHCDA049E "Write KD orientation error"
#define HHCDA050I "Updating cyl(%d) head(%d) record(%d) kl(%d) dl(%d)"
#define HHCDA051E "Write data orientation error"
#define HHCDA052I "Updating cyl(%d) head(%d) record(%d) dl(%d)"
#define HHCDA053E "Data chaining not supported for CCW(%2.2X)"
#define HHCDA054I "Set file mask(%2.2X)"
#define HHCDA055I "Search key(%s)"

/* cmdtab.c */
#define HHCPN139E "Command(%s) not found, enter '?' for list"
#define HHCPN140I "Valid panel commands are\n  %-9.9s    %s \n  %-9.9s    %s "
#define HHCPN142I "Command(%s) not found - no help available"

/* codepage.c */
#define HHCCF072I "Using %s codepage conversion table %s"
#define HHCCF051E "Codepage conversion table %s is not defined"

/* comm3705.c and commadpt.c */
#define HHCCA001I "Client(%s) connected to %4.4X device(%4.4X)"
#define HHCCA002I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCCA003E "Device(%4.4X) Cannot obtain socket for incoming calls: %s"
#define HHCCA004W "Device(%4.4X) Waiting 5 seconds for port(%d) to become available"
#define HHCCA005I "Device(%4.4X) Listening on port(%d) for incoming TCP connections"
#define HHCCA006T "Device(%4.4X) Select failed: %s"
#define HHCCA007W "Device(%4.4X) Outgoing call failed during %s command: %s"
#define HHCCA008I "Device(%4.4X) Incoming Call"
#define HHCCA009I "Thread ended tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCCA010I "device(%4.4X) initialisation not performed"
#define HHCCA011E "Device(%4.4X) Error parsing %s"
#define HHCCA012E "Device(%4.4X) Unrecognized parameter(%s)"
#define HHCCA013E "Device(%4.4X) Incorrect %s specification %s"
#define HHCCA014E "Device(%4.4X) Incorrect switched/dial specification(%s): defaulting to DIAL=OUT"
#define HHCCA015E "Device(%4.4X) Missing parameter: DIAL(%s) and %s not specified"
#define HHCCA016W "Device(%4.4X) Conflicting parameter: DIAL(%s) and %s=%s specified"
#define HHCCA017I "Device(%4.4X) RPORT parameter ignored"
#define HHCCA018E "Device(%4.4X) Bind failed: %s"
#define HHCCA019E "Device(%4.4X) BSC comm thread did not initialise"
#define HHCCA020E "Device(%4.4X) Memory allocation failure for main control block"
#define HHCCA021I "Device(%4.4X) Initialisation failed due to previous errors"
#define HHCCA022E "Cannot create commadpt thread: %s"
#define HHCCA023I "Device(%4.4X) Connect out to %s:%d failed during initial status: %s"

/* con1052c.c */
#define HHC1C001A "Enter input for console device(%4.4X)"

/* config.c */
#define HHCCF040E "Cannot create %s%02X thread: %s"
#define HHCCF041E "Device(%d:%4.4X) already exists"
#define HHCCF042E "Devtyp(%s) not recognized"
#define HHCCF043E "Cannot obtain device block: %s"
#define HHCCF044E "Initialization failed for device(%4.4X)"
#define HHCCF045E "Cannot obtain buffer for device(%4.4X): %s"
#define HHCCF046E "%s(%d:%4.4X) does not exist"
#define HHCCF047I "%s(%d:%4.4X) detached"
#define HHCCF048E "Device(%d:%4.4X) does not exist"
#define HHCCF049E "Device(%d:%4.4X) already exists"
#define HHCCF053E "Incorrect second device number in device range near character(%c)"
#define HHCCF054E "Incorrect Device count near character(%c)"
#define HHCCF055E "Incorrect device address specification near character(%c)"
#define HHCCF056E "Incorrect device address range(%4.4X < %4.4X)"
#define HHCCF057E "Device(%4.4X_ is on wrong channel (1st device defined on channel(%2.2X))"
#define HHCCF058E "Some or all devices in %4.4X-%4.4X duplicate devices already defined"
#define HHCCF074E "Unspecified error occured while parsing Logical Channel Subsystem Identification"
#define HHCCF075E "No more than 1 Logical Channel Subsystem Identification may be specified"
#define HHCCF076E "Non numeric Logical Channel Subsystem Identification(%s)"
#define HHCCF077E "Logical Channel Subsystem Identification(%d) exceeds maximum(%d)"

/* console.c and comm3705.c */
#define HHCGI001I "Unable to determine IP address from (%s)"
#define HHCGI002I "Unable to determine port number from (%s)"
#define HHCGI003E "Invalid parameter(%s)"

#define HHCTE001I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCTE002W "Waiting for port(%u) to become free"
#define HHCTE003I "Waiting for console connection on port(%u)"
#define HHCTE004I "Thread ended tid(" TIDPAT ") pid(%d) prio(%d) name(%s)" 
#define HHCTE005E "Cannot create console thread: (%s)"
#define HHCTE007I "Device(%4.4X): Devtype(%4.4X) client(%s) connection closed"
#define HHCTE008I "Device(%4.4X): Connection closed by client(%s)"
#define HHCTE009I "Client(%s) connected to device(%d:%4.4X) devtype(%4.4X)"
#define HHCTE010E "CNSLPORT statement invalid: (%s)"
#define HHCTE011E "Device(%4.4X): Invalid IP address: (%s)"
#define HHCTE012E "Device(%4.4X): Invalid mask value: (%s)"
#define HHCTE013E "Device(%4.4X): Extraneous argument(s): (%s)..."
#define HHCTE014I "Device(%4.4X): Devtype(%4.4X) client(%s) connection reset"
#define HHCTE017E "Device(%4.4X): Duplicate SYSG console definition"
#define HHCTE090E "Device(%4.4X): Malloc() failed for resume buf: (%s)"

/* cpu.c */
#define HHCCP001W "%s%02X thread set priority(%d) failed: (%s)"
#define HHCCP002I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s%02X)"
#define HHCCP003I "%s%02X architecture mode(%s)"
#define HHCCP004I "%s%02X Vector Facility online"
#define HHCCP006S "Cannot create timer thread: (%s)"
#define HHCCP007I "%s%02X architecture mode set to (%s)"
#define HHCCP008I "Thread ended tid(" TIDPAT ") pid(%d) prio(%d) name(%s%02X)"
#define HHCCP010I "%s%02X store status completed"
#define HHCCP011I "%s%02X disabled wait state(%s)"
#define HHCCP014I "%s%s%s%02X: (%s) code(%4.4X) ilc(%d%s)"
#define HHCCP015I "%s%02X PER event: code(%4.4X) perc(%2.2X) addr(" F_VADR ")"
#define HHCCP016I "%s%02X: Program interrupt loop: PSW(%s)"
#define HHCCP022I "Machine Check code (%16.16" I64_FMT "u)"
#define HHCCP043I "Wait state PSW loaded: (%s)"
#define HHCCP044I "I/O interrupt code(%4.4X) CSW(%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X)"
#define HHCCP045I "I/O interrupt code(%8.8X) parm(%8.8X)"
#define HHCCP046I "I/O interrupt code(%8.8X0 parm(%8.8X), id(%8.8X)"
#define HHCCP080E "%s%02X malloc failed for archjmp regs: (%s)"

/* crypto/dyncrypt.c */
#define HHCRY001I "Crypto module loaded (c) Copyright Bernard van der Helm, 2003-2010"
#define HHCRY002I "  Active: %s"

/* ctcadpt.c */
#define HHCCT001E "Device(%4.4X) Incorrect number of parameters"
#define HHCCT002E "Device(%4.4X) Incorrect number of parameters"
#define HHCCT003E "Device(%4.4X) Invalid port number(%s)"
#define HHCCT004E "Device(%4.4X) Invalid IP address(%s)"
#define HHCCT005E "Device(%4.4X) Invalid port number(%s)"
#define HHCCT006E "Device(%4.4X) Invalid MTU size(%s)"
#define HHCCT007E "Device(%4.4X) Error creating socket(%s)"
#define HHCCT008E "Device(%4.4X) Error binding socket(%s)"
#define HHCCT009I "Device(%4.4X) Connect to %s:%s failed, starting server"
#define HHCCT010E "Device(%4.4X) Error creating socket: %s"
#define HHCCT011E "Device(%4.4X) Error binding socket(%s)"
#define HHCCT012E "Device(%4.4X) Error on call to listen: %s"
#define HHCCT013I "Device(%4.4X) Connected to %s:%s"
#define HHCCT014E "Device(%4.4X) Write CCW count %u is invalid"
#define HHCCT015I "Device(%4.4X) Interface command: %s %8.8X"
#define HHCCT016E "Device(%4.4X) Write buffer contains incomplete segment header at offset %4.4X"
#define HHCCT017E "Device(%4.4X) Write buffer contains invalid segment length %u at offset %4.4X"
#define HHCCT018I "Device(%4.4X) Sending packet to file(%s)"
#define HHCCT019E "Device(%4.4X) Error writing to file(%s): %s"
#define HHCCT020E "Device(%4.4X) Error reading from file(%s): %s"
#define HHCCT021E "Device(%4.4X) Error reading from file(%s): %s"
#define HHCCT022I "Device(%4.4X) Received packet from %s (%d bytes)"
#define HHCCT023E "Device(%4.4X) Incorrect client or config error: Config(%s) connecting client(%s)"
#define HHCCT024E "Device(%4.4X) Not enough arguments to start vmnet"
#define HHCCT025E "Device(%4.4X) Failed: socketpair: %s"
#define HHCCT026E "Device(%4.4X) Failed: fork: %s"
#define HHCCT027E "Device(%4.4X) Not enough parameters"
#define HHCCT028E "Device(%d:%4.4X): Bad device number(%s)"
#define HHCCT029E "Device(%4.4X) bad block length: %d < %d"
#define HHCCT030E "Device(%4.4X) bad packet length: %d < %d"
#define HHCCT032E "Device(%4.4X) Error: EOF on read, CTC network down"
#define HHCCT033E "Device(%4.4X) Error: read: %s"
#define HHCCT034E "Unrecognized/unsupported CTC emulation type(%s)"

/* hao.c */
#define HHCAO001I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCAO002I "Thread ended tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCAO003I "Firing command: (%s)"
#define HHCAO004I "The defined Automatic Operator rule(s) are:"
#define HHCAO005I "(%02d): (%s) -> (%s)"
#define HHCAO006I "(%d) rule(s) displayed"
#define HHCAO007E "Unknown hao command, valid commands are:\n" \
                  "          hao tgt <tgt> : define target rule (pattern) to react on\n" \
                  "          hao cmd <cmd> : define command for previously defined rule\n" \
                  "          hao list <n>  : list all rules/commands or only at index <n>\n" \
                  "          hao del <n>   : delete the rule at index <n>\n" \
                  "          hao clear     : delete all rules (stops automatic operator)"
#define HHCAO008E "No rule defined at index(%d)"
#define HHCAO009E "Invalid index, index must be between 0 and (%d)"
#define HHCAO010E "Target not added, table full"
#define HHCAO011E "Tgt command given, but cmd command expected"
#define HHCAO012E "Empty target specified"
#define HHCAO013E "Target not added, duplicate found in table"
#define HHCAO014E "%s"
#define HHCAO015E "%s"
#define HHCAO016I "Target placed at index(%d)"
#define HHCAO017E "Cmd command given, but tgt command expected"
#define HHCAO018E "Empty command specified"
#define HHCAO019E "Command not added; causes loop with target at index(%d)"
#define HHCAO020I "Command placed at index(%d)"
#define HHCAO021E "Target not added, causes loop with command at index(%d)"
#define HHCAO022I "All automatic operation rules cleared"
#define HHCAO023E "Hao del command given without a valid index"
#define HHCAO024E "Rule at index(%d) not deleted, already empty"
#define HHCAO025I "Rule at index(%d) succesfully deleted"
#define HHCA0026E "Command not added, may cause dead locks"

/* hdl.c */
#define HHCHD001E "Registration malloc failed for (%s)"
#define HHCHD005E "Module(%s) already loaded\n"
#define HHCHD006S "Cannot allocate memory for DLL descriptor: (%s)"
#define HHCHD007E "Unable to open DLL(%s): (%s)"
#define HHCHD008E "Device(%4.4X) bound to (%s)"
#define HHCHD009E "Module(%s) not found"
#define HHCHD010I "Dependency check failed for (%s), version(%s) expected(%s)"
#define HHCHD011I "Dependency check failed for (%s), size(%d) expected(%d)"
#define HHCHD013E "No dependency section in (%s): (%s)"
#define HHCHD014E "Dependency check failed for module(%s)"
#define HHCHD015E "Unloading of module(%s) not allowed"
#define HHCHD016E "DLL(%s) is duplicate of (%s)"
#define HHCHD017E "Unload of module(%s) rejected by final section"
#define HHCHD018I "Loadable module directory is (%s)"
#define HHCHD900I "Begin shutdown sequence"
#define HHCHD901I "Calling (%s)"
#define HHCHD902I "(%s) complete"
#define HHCHD903I "(%s) skipped during Windows SHUTDOWN immediate"
#define HHCHD909I "Shutdown sequence complete"
#define HHCHD950I "Begin HDL termination sequence"
#define HHCHD951I "Calling module(%s) cleanup routine"
#define HHCHD952I "Module(%s) cleanup complete"
#define HHCHD959I "HDL Termination sequence complete"

/* hostinfo.c */
#define HHCIN015I "%s"

/* httpserv.c */
#define HHCHT001I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCHT002E "Socket error: (%s)"
#define HHCHT003W "Waiting for port(%u) to become free"
#define HHCHT004E "Bind error: (%s)"
#define HHCHT005E "Listen error: (%s)"
#define HHCHT006I "Waiting for HTTP requests on port(%u)"
#define HHCHT007E "Select error: (%s)"
#define HHCHT008E "Accept error: (%s)"
#define HHCHT009I "Thread ended tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCHT010E "Http_request create_thread error: (%s)"
#define HHCHT011E "Html_include: Cannot open file(%s): (%s)"
#define HHCHT013I "Using HTTPROOT directory (%s)"
#define HHCCF066E "Invalid HTTPROOT: (%s): (%s)"

/* impl.c */
#define HHCIN001S "Cannot register SIGINT handler: %s"
#define HHCIN002E "Cannot suppress SIGPIPE signal: %s"
#define HHCIN003S "Cannot register SIGILL/FPE/SEGV/BUS/USR handler: %s"
#define HHCIN004S "Cannot create HAO thread: %s"
#define HHCIN005S "Cannot create watchdog thread: %s"
#define HHCIN006S "Cannot create shared_server thread: %s"
#define HHCIN007S "Cannot create %4.4X connection thread: %s"
#define HHCIN008S "DYNGUI.DLL load failed; Hercules terminated."
#define HHCIN009S "Cannot register SIGTERM handler: %s"
#define HHCIN010S "Cannot register ConsoleCtrl handler: %s"
#define HHCIN021I "CLOSE Event received, SHUTDOWN Immediate starting..."
#define HHCIN022I "Ctrl-C intercepted"
#define HHCIN023W "CLOSE Event received, SHUTDOWN previously requested..."
#define HHCIN050I "Ctrl-Break intercepted. Interrupt Key depressed simulated."
#define HHCIN099I "Hercules terminated"
#define HHCIN950I "Begin system cleanup"
#define HHCIN959I "System cleanup complete"
#define HHCPN995E ".RC file \"%s\" not found."

/* panel.c */
#define HHCPN001I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCPN002S "Cannot obtain keyboard buffer: (%s)"
#define HHCPN003S "Cannot obtain message buffer: (%s)"

/* timer.c */
#define HHCTT001W "Timer thread set priority(%d) failed: (%s)"
#define HHCTT002I "Thread started tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"
#define HHCTT003I "Thread ended tid(" TIDPAT ") pid(%d) prio(%d) name(%s)"

/* version.c */
#define HHCIN010I "%sversion (%s)"
#define HHCIN011I "%s"
#define HHCIN012I "Built on (%s) at (%s)"
#define HHCIN013I "Build information:"
#define HHCIN014I "  %s"
