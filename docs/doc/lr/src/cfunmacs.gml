.* MACROS USED WITH LIBRARY DESCRIPTIONS (C AND GRAPHICS LIBRARIES)
.*
.* fd... macros are used for function definitions
.* df... macros are used for header file definitions
.* sf... macros are used for structure/union definitions
.*
.dm fdbeg begin
The following functions are defined:
.if &e'&dohelp eq 0 .do begin
:DL compact termhi=1 tsize=14.
.do end
.el .do begin
:ZDL compact termhi=1 tsize=14.
.do end
.dm fdbeg end
.*
.*  The hotlink depends on the fact that the first name in the list
.*  of functions described by a single file is also the first name
.*  listed in the .func line of that single file.  If this is not the
.*  case then the hotlink will not work.
.*
.dm fd begin
.se *fnd=&'vecpos(&*fun.,fnclst)
.if &*fnd. ne 0 .do begin
.   .se *lib=&'vecpos(&imblst(&*fnd.).,imblst)
.   .se *ent=&fnclst(&*lib.)
.*  there are 8 characters in <*fun="" >
.   .se *dsc=&'substr(&*,9+&'length(&*fun.))
.   .if &e'&dohelp eq 0 .do begin
:DT.&*fun.
:DD.&*dsc.
.   .do end
.   .el .do begin
.   .   .if &'compare('_&*fun.', &*ent.) eq 0 .do begin
.   .   .   .ty ***WARNING*** check order of &*fun., &*ent.
.   .   .do end
.   .   .if &'compare(&*fun., &*ent.) eq 0 .do begin
:ZDT.:QREF str='&*fun.'.
.   .   .do end
.   .   .el .do begin
:ZDT.&*fun. (see :QREF str='&*ent.'.)
.   .   .do end
:ZDD.&*dsc.
.   .do end
.do end
.dm fd end
.*
.dm fdend begin
.if &e'&dohelp eq 0 .do begin
:eDL.
.do end
.el .do begin
:ZeDL.
.do end
.dm fdend end
.*
.dm dfbeg begin
.if &e'&dohelp eq 0 .do begin
:DL break termhi=3.
.do end
.el .do begin
:ZDL break termhi=3.
.do end
.dm dfbeg end
.*
.dm df begin
.ix '&*'
.ix 'data' '&*'
.if &e'&dohelp eq 0 .do begin
:DT.&*
:DD.
.do end
.el .do begin
:ZDT.&*
:ZDD.
.do end
.dm df end
.*
.dm dfend begin
.if &e'&dohelp eq 0 .do begin
:eDL.
.do end
.el .do begin
:ZeDL.
.do end
.dm dfend end
.*
.dm sfbeg begin
.if &e'&dohelp eq 0 .do begin
:DL termhi=3.
.do end
.el .do begin
:ZDL termhi=3.
.do end
.dm sfbeg end
.*
.dm sf begin
.ix '&*'
.ix 'structure' '&*'
.if &e'&dohelp eq 0 .do begin
:DT.&*
:DD.
.do end
.el .do begin
:ZDT.&*
:ZDD.
.do end
.dm sf end
.*
.dm sfend begin
.if &e'&dohelp eq 0 .do begin
:eDL.
.do end
.el .do begin
:ZeDL.
.do end
.dm sfend end
.*
