$ defs = "/define=("USE_LARGEFILES","ABORT_DEEP_ISO_ONLY","APPLE_HYB","UDF","DVD_VIDEO","SORTING","USE_LIBSCHILY","USE_SCG","HAVE_DIRENT_H","HAVE_STRCASECMP")
$ incs = "/include=([-.include],[-.libhfs_iso],[-.cdrecord],[])"
$ opts = "/float=ieee/prefix=all"
$ define/nolog scg [-.LIBSCG.scg]
$ cc 'defs' 'incs'  'opts' NLS_BASE.C
$ cc 'defs' 'incs'  'opts' NLS_CONFIG.C
$ cc 'defs' 'incs'  'opts' NLS_CP10000.C
$ cc 'defs' 'incs'  'opts' NLS_CP10006.C
$ cc 'defs' 'incs'  'opts' NLS_CP10007.C
$ cc 'defs' 'incs'  'opts' NLS_CP10029.C
$ cc 'defs' 'incs'  'opts' NLS_CP10079.C
$ cc 'defs' 'incs'  'opts' NLS_CP10081.C
$ cc 'defs' 'incs'  'opts' NLS_CP1250.C
$ cc 'defs' 'incs'  'opts' NLS_CP1251.C
$ cc 'defs' 'incs'  'opts' NLS_CP437.C
$ cc 'defs' 'incs'  'opts' NLS_CP737.C
$ cc 'defs' 'incs'  'opts' NLS_CP775.C
$ cc 'defs' 'incs'  'opts' NLS_CP850.C
$ cc 'defs' 'incs'  'opts' NLS_CP852.C
$ cc 'defs' 'incs'  'opts' NLS_CP855.C
$ cc 'defs' 'incs'  'opts' NLS_CP857.C
$ cc 'defs' 'incs'  'opts' NLS_CP860.C
$ cc 'defs' 'incs'  'opts' NLS_CP861.C
$ cc 'defs' 'incs'  'opts' NLS_CP862.C
$ cc 'defs' 'incs'  'opts' NLS_CP863.C
$ cc 'defs' 'incs'  'opts' NLS_CP864.C
$ cc 'defs' 'incs'  'opts' NLS_CP865.C
$ cc 'defs' 'incs'  'opts' NLS_CP866.C
$ cc 'defs' 'incs'  'opts' NLS_CP869.C
$ cc 'defs' 'incs'  'opts' NLS_CP874.C
$ cc 'defs' 'incs'  'opts' NLS_FILE.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-1.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-14.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-15.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-2.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-3.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-4.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-5.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-6.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-7.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-8.C
$ cc 'defs' 'incs'  'opts' NLS_ISO8859-9.C
$ cc 'defs' 'incs'  'opts' NLS_KOI8-R.C
$ cc 'defs' 'incs'  'opts' NLS_KOI8-U.C
$ libr/cre [-.libs]LIBUNLS.olb
$ libr/ins [-.libs]LIBUNLS.olb *.obj         
$ delete *.obj;*
$ purge/nolog [-.libs]*.olb
