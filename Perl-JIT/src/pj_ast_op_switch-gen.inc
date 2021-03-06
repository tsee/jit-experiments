#ifndef PJ_AST_OP_SWITCH_GEN_INC_
#define PJ_AST_OP_SWITCH_GEN_INC_
// WARNING: Do not modify this file, it is generated!
// Modify the generating script author_tools/regen_opcodes.pl and its data file
// "author_tools/opcodes" instead!

// Macros to determine which Perl OPs to ASTify

#define IS_AST_COMPATIBLE_ROOT_OP_TYPE(otype) ( \
     (int)otype == OP_AASSIGN \
  || (int)otype == OP_ABS \
  || (int)otype == OP_ACCEPT \
  || (int)otype == OP_ADD \
  || (int)otype == OP_AEACH \
  || (int)otype == OP_AELEM \
  || (int)otype == OP_AKEYS \
  || (int)otype == OP_ALARM \
  || (int)otype == OP_ANDASSIGN \
  || (int)otype == OP_ANONCODE \
  || (int)otype == OP_ANONHASH \
  || (int)otype == OP_ANONLIST \
  || (int)otype == OP_ASLICE \
  || (int)otype == OP_ATAN2 \
  || (int)otype == OP_AV2ARYLEN \
  || (int)otype == OP_AVALUES \
  || (int)otype == OP_BACKTICK \
  || (int)otype == OP_BIND \
  || (int)otype == OP_BINMODE \
  || (int)otype == OP_BIT_AND \
  || (int)otype == OP_BIT_OR \
  || (int)otype == OP_BIT_XOR \
  || (int)otype == OP_BLESS \
  || (int)otype == OP_BREAK \
  || (int)otype == OP_CALLER \
  || (int)otype == OP_CHDIR \
  || (int)otype == OP_CHMOD \
  || (int)otype == OP_CHOMP \
  || (int)otype == OP_CHOP \
  || (int)otype == OP_CHOWN \
  || (int)otype == OP_CHR \
  || (int)otype == OP_CHROOT \
  || (int)otype == OP_CLOSE \
  || (int)otype == OP_CLOSEDIR \
  || (int)otype == OP_COMPLEMENT \
  || (int)otype == OP_CONCAT \
  || (int)otype == OP_COND_EXPR \
  || (int)otype == OP_CONNECT \
  || (int)otype == OP_CONTINUE \
  || (int)otype == OP_COS \
  || (int)otype == OP_CRYPT \
  || (int)otype == OP_DBMCLOSE \
  || (int)otype == OP_DBMOPEN \
  || (int)otype == OP_DEFINED \
  || (int)otype == OP_DELETE \
  || (int)otype == OP_DIE \
  || (int)otype == OP_DIVIDE \
  || (int)otype == OP_DOFILE \
  || (int)otype == OP_DORASSIGN \
  || (int)otype == OP_DUMP \
  || (int)otype == OP_EACH \
  || (int)otype == OP_EGRENT \
  || (int)otype == OP_EHOSTENT \
  || (int)otype == OP_ENETENT \
  || (int)otype == OP_ENTERSUB \
  || (int)otype == OP_EOF \
  || (int)otype == OP_EPROTOENT \
  || (int)otype == OP_EPWENT \
  || (int)otype == OP_EQ \
  || (int)otype == OP_ESERVENT \
  || (int)otype == OP_EXEC \
  || (int)otype == OP_EXISTS \
  || (int)otype == OP_EXIT \
  || (int)otype == OP_EXP \
  || (int)otype == OP_FC \
  || (int)otype == OP_FCNTL \
  || (int)otype == OP_FILENO \
  || (int)otype == OP_FLOCK \
  || (int)otype == OP_FORK \
  || (int)otype == OP_FORMLINE \
  || (int)otype == OP_FTATIME \
  || (int)otype == OP_FTBINARY \
  || (int)otype == OP_FTBLK \
  || (int)otype == OP_FTCHR \
  || (int)otype == OP_FTCTIME \
  || (int)otype == OP_FTDIR \
  || (int)otype == OP_FTEEXEC \
  || (int)otype == OP_FTEOWNED \
  || (int)otype == OP_FTEREAD \
  || (int)otype == OP_FTEWRITE \
  || (int)otype == OP_FTFILE \
  || (int)otype == OP_FTIS \
  || (int)otype == OP_FTLINK \
  || (int)otype == OP_FTMTIME \
  || (int)otype == OP_FTPIPE \
  || (int)otype == OP_FTREXEC \
  || (int)otype == OP_FTROWNED \
  || (int)otype == OP_FTRREAD \
  || (int)otype == OP_FTRWRITE \
  || (int)otype == OP_FTSGID \
  || (int)otype == OP_FTSIZE \
  || (int)otype == OP_FTSOCK \
  || (int)otype == OP_FTSUID \
  || (int)otype == OP_FTSVTX \
  || (int)otype == OP_FTTEXT \
  || (int)otype == OP_FTTTY \
  || (int)otype == OP_FTZERO \
  || (int)otype == OP_GE \
  || (int)otype == OP_GELEM \
  || (int)otype == OP_GETC \
  || (int)otype == OP_GETLOGIN \
  || (int)otype == OP_GETPEERNAME \
  || (int)otype == OP_GETPGRP \
  || (int)otype == OP_GETPPID \
  || (int)otype == OP_GETPRIORITY \
  || (int)otype == OP_GETSOCKNAME \
  || (int)otype == OP_GGRENT \
  || (int)otype == OP_GGRGID \
  || (int)otype == OP_GGRNAM \
  || (int)otype == OP_GHBYADDR \
  || (int)otype == OP_GHBYNAME \
  || (int)otype == OP_GHOSTENT \
  || (int)otype == OP_GLOB \
  || (int)otype == OP_GMTIME \
  || (int)otype == OP_GNBYADDR \
  || (int)otype == OP_GNBYNAME \
  || (int)otype == OP_GNETENT \
  || (int)otype == OP_GOTO \
  || (int)otype == OP_GPBYNAME \
  || (int)otype == OP_GPBYNUMBER \
  || (int)otype == OP_GPROTOENT \
  || (int)otype == OP_GPWENT \
  || (int)otype == OP_GPWNAM \
  || (int)otype == OP_GPWUID \
  || (int)otype == OP_GREPWHILE \
  || (int)otype == OP_GSBYNAME \
  || (int)otype == OP_GSBYPORT \
  || (int)otype == OP_GSERVENT \
  || (int)otype == OP_GSOCKOPT \
  || (int)otype == OP_GT \
  || (int)otype == OP_HELEM \
  || (int)otype == OP_HEX \
  || (int)otype == OP_HSLICE \
  || (int)otype == OP_INDEX \
  || (int)otype == OP_INT \
  || (int)otype == OP_IOCTL \
  || (int)otype == OP_I_ADD \
  || (int)otype == OP_I_DIVIDE \
  || (int)otype == OP_I_EQ \
  || (int)otype == OP_I_GE \
  || (int)otype == OP_I_GT \
  || (int)otype == OP_I_LE \
  || (int)otype == OP_I_LT \
  || (int)otype == OP_I_MODULO \
  || (int)otype == OP_I_MULTIPLY \
  || (int)otype == OP_I_NCMP \
  || (int)otype == OP_I_NE \
  || (int)otype == OP_I_NEGATE \
  || (int)otype == OP_I_POSTDEC \
  || (int)otype == OP_I_POSTINC \
  || (int)otype == OP_I_PREDEC \
  || (int)otype == OP_I_PREINC \
  || (int)otype == OP_I_SUBTRACT \
  || (int)otype == OP_JOIN \
  || (int)otype == OP_KEYS \
  || (int)otype == OP_KILL \
  || (int)otype == OP_LC \
  || (int)otype == OP_LCFIRST \
  || (int)otype == OP_LE \
  || (int)otype == OP_LEAVE \
  || (int)otype == OP_LEAVEGIVEN \
  || (int)otype == OP_LEAVELOOP \
  || (int)otype == OP_LEAVEWHEN \
  || (int)otype == OP_LEFT_SHIFT \
  || (int)otype == OP_LENGTH \
  || (int)otype == OP_LINK \
  || (int)otype == OP_LIST \
  || (int)otype == OP_LISTEN \
  || (int)otype == OP_LOCALTIME \
  || (int)otype == OP_LOCK \
  || (int)otype == OP_LOG \
  || (int)otype == OP_LSLICE \
  || (int)otype == OP_LSTAT \
  || (int)otype == OP_LT \
  || (int)otype == OP_MAPWHILE \
  || (int)otype == OP_MATCH \
  || (int)otype == OP_METHOD \
  || (int)otype == OP_METHOD_NAMED \
  || (int)otype == OP_MKDIR \
  || (int)otype == OP_MODULO \
  || (int)otype == OP_MSGCTL \
  || (int)otype == OP_MSGGET \
  || (int)otype == OP_MSGRCV \
  || (int)otype == OP_MSGSND \
  || (int)otype == OP_MULTIPLY \
  || (int)otype == OP_NCMP \
  || (int)otype == OP_NE \
  || (int)otype == OP_NEGATE \
  || (int)otype == OP_NOT \
  || (int)otype == OP_OCT \
  || (int)otype == OP_ONCE \
  || (int)otype == OP_OPEN \
  || (int)otype == OP_OPEN_DIR \
  || (int)otype == OP_ORASSIGN \
  || (int)otype == OP_ORD \
  || (int)otype == OP_PACK \
  || (int)otype == OP_PADAV \
  || (int)otype == OP_PADHV \
  || (int)otype == OP_PADSV \
  || (int)otype == OP_PIPE_OP \
  || (int)otype == OP_POP \
  || (int)otype == OP_POS \
  || (int)otype == OP_POSTDEC \
  || (int)otype == OP_POSTINC \
  || (int)otype == OP_POW \
  || (int)otype == OP_PREDEC \
  || (int)otype == OP_PREINC \
  || (int)otype == OP_PRINT \
  || (int)otype == OP_PROTOTYPE \
  || (int)otype == OP_PRTF \
  || (int)otype == OP_PUSH \
  || (int)otype == OP_QUOTEMETA \
  || (int)otype == OP_RAND \
  || (int)otype == OP_REACH \
  || (int)otype == OP_READ \
  || (int)otype == OP_READDIR \
  || (int)otype == OP_READLINE \
  || (int)otype == OP_READLINK \
  || (int)otype == OP_RECV \
  || (int)otype == OP_REF \
  || (int)otype == OP_REFGEN \
  || (int)otype == OP_RENAME \
  || (int)otype == OP_REPEAT \
  || (int)otype == OP_REQUIRE \
  || (int)otype == OP_RESET \
  || (int)otype == OP_RETURN \
  || (int)otype == OP_REVERSE \
  || (int)otype == OP_REWINDDIR \
  || (int)otype == OP_RIGHT_SHIFT \
  || (int)otype == OP_RINDEX \
  || (int)otype == OP_RKEYS \
  || (int)otype == OP_RMDIR \
  || (int)otype == OP_RV2AV \
  || (int)otype == OP_RV2CV \
  || (int)otype == OP_RV2GV \
  || (int)otype == OP_RV2HV \
  || (int)otype == OP_RV2SV \
  || (int)otype == OP_RVALUES \
  || (int)otype == OP_SASSIGN \
  || (int)otype == OP_SAY \
  || (int)otype == OP_SCHOMP \
  || (int)otype == OP_SCHOP \
  || (int)otype == OP_SCMP \
  || (int)otype == OP_SCOPE \
  || (int)otype == OP_SEEK \
  || (int)otype == OP_SEEKDIR \
  || (int)otype == OP_SELECT \
  || (int)otype == OP_SEMCTL \
  || (int)otype == OP_SEMGET \
  || (int)otype == OP_SEMOP \
  || (int)otype == OP_SEND \
  || (int)otype == OP_SEQ \
  || (int)otype == OP_SETPGRP \
  || (int)otype == OP_SETPRIORITY \
  || (int)otype == OP_SGE \
  || (int)otype == OP_SGRENT \
  || (int)otype == OP_SGT \
  || (int)otype == OP_SHIFT \
  || (int)otype == OP_SHMCTL \
  || (int)otype == OP_SHMGET \
  || (int)otype == OP_SHMREAD \
  || (int)otype == OP_SHMWRITE \
  || (int)otype == OP_SHOSTENT \
  || (int)otype == OP_SHUTDOWN \
  || (int)otype == OP_SIN \
  || (int)otype == OP_SLE \
  || (int)otype == OP_SLEEP \
  || (int)otype == OP_SLT \
  || (int)otype == OP_SMARTMATCH \
  || (int)otype == OP_SNE \
  || (int)otype == OP_SNETENT \
  || (int)otype == OP_SOCKET \
  || (int)otype == OP_SOCKPAIR \
  || (int)otype == OP_SORT \
  || (int)otype == OP_SPLICE \
  || (int)otype == OP_SPLIT \
  || (int)otype == OP_SPRINTF \
  || (int)otype == OP_SPROTOENT \
  || (int)otype == OP_SPWENT \
  || (int)otype == OP_SQRT \
  || (int)otype == OP_SRAND \
  || (int)otype == OP_SREFGEN \
  || (int)otype == OP_SSELECT \
  || (int)otype == OP_SSERVENT \
  || (int)otype == OP_SSOCKOPT \
  || (int)otype == OP_STAT \
  || (int)otype == OP_STUB \
  || (int)otype == OP_STUDY \
  || (int)otype == OP_SUBST \
  || (int)otype == OP_SUBSTR \
  || (int)otype == OP_SUBTRACT \
  || (int)otype == OP_SYMLINK \
  || (int)otype == OP_SYSCALL \
  || (int)otype == OP_SYSOPEN \
  || (int)otype == OP_SYSREAD \
  || (int)otype == OP_SYSSEEK \
  || (int)otype == OP_SYSTEM \
  || (int)otype == OP_SYSWRITE \
  || (int)otype == OP_TELL \
  || (int)otype == OP_TELLDIR \
  || (int)otype == OP_TIED \
  || (int)otype == OP_TIME \
  || (int)otype == OP_TMS \
  || (int)otype == OP_TRUNCATE \
  || (int)otype == OP_UC \
  || (int)otype == OP_UCFIRST \
  || (int)otype == OP_UMASK \
  || (int)otype == OP_UNDEF \
  || (int)otype == OP_UNLINK \
  || (int)otype == OP_UNPACK \
  || (int)otype == OP_UNSHIFT \
  || (int)otype == OP_UNTIE \
  || (int)otype == OP_UTIME \
  || (int)otype == OP_VALUES \
  || (int)otype == OP_VEC \
  || (int)otype == OP_WAIT \
  || (int)otype == OP_WAITPID \
  || (int)otype == OP_WANTARRAY \
  || (int)otype == OP_WARN)


#define IS_AST_COMPATIBLE_OP_TYPE(otype) ( \
     (int)otype == OP_AASSIGN \
  || (int)otype == OP_ABS \
  || (int)otype == OP_ACCEPT \
  || (int)otype == OP_ADD \
  || (int)otype == OP_AEACH \
  || (int)otype == OP_AELEM \
  || (int)otype == OP_AELEMFAST \
  || (int)otype == OP_AELEMFAST_LEX \
  || (int)otype == OP_AKEYS \
  || (int)otype == OP_ALARM \
  || (int)otype == OP_AND \
  || (int)otype == OP_ANDASSIGN \
  || (int)otype == OP_ANONCODE \
  || (int)otype == OP_ANONHASH \
  || (int)otype == OP_ANONLIST \
  || (int)otype == OP_ASLICE \
  || (int)otype == OP_ATAN2 \
  || (int)otype == OP_AV2ARYLEN \
  || (int)otype == OP_AVALUES \
  || (int)otype == OP_BACKTICK \
  || (int)otype == OP_BIND \
  || (int)otype == OP_BINMODE \
  || (int)otype == OP_BIT_AND \
  || (int)otype == OP_BIT_OR \
  || (int)otype == OP_BIT_XOR \
  || (int)otype == OP_BLESS \
  || (int)otype == OP_BREAK \
  || (int)otype == OP_CALLER \
  || (int)otype == OP_CHDIR \
  || (int)otype == OP_CHMOD \
  || (int)otype == OP_CHOMP \
  || (int)otype == OP_CHOP \
  || (int)otype == OP_CHOWN \
  || (int)otype == OP_CHR \
  || (int)otype == OP_CHROOT \
  || (int)otype == OP_CLOSE \
  || (int)otype == OP_CLOSEDIR \
  || (int)otype == OP_COMPLEMENT \
  || (int)otype == OP_CONCAT \
  || (int)otype == OP_COND_EXPR \
  || (int)otype == OP_CONNECT \
  || (int)otype == OP_CONST \
  || (int)otype == OP_CONTINUE \
  || (int)otype == OP_COS \
  || (int)otype == OP_CRYPT \
  || (int)otype == OP_DBMCLOSE \
  || (int)otype == OP_DBMOPEN \
  || (int)otype == OP_DEFINED \
  || (int)otype == OP_DELETE \
  || (int)otype == OP_DIE \
  || (int)otype == OP_DIVIDE \
  || (int)otype == OP_DOFILE \
  || (int)otype == OP_DOR \
  || (int)otype == OP_DORASSIGN \
  || (int)otype == OP_DUMP \
  || (int)otype == OP_EACH \
  || (int)otype == OP_EGRENT \
  || (int)otype == OP_EHOSTENT \
  || (int)otype == OP_ENETENT \
  || (int)otype == OP_ENTERSUB \
  || (int)otype == OP_EOF \
  || (int)otype == OP_EPROTOENT \
  || (int)otype == OP_EPWENT \
  || (int)otype == OP_EQ \
  || (int)otype == OP_ESERVENT \
  || (int)otype == OP_EXEC \
  || (int)otype == OP_EXISTS \
  || (int)otype == OP_EXIT \
  || (int)otype == OP_EXP \
  || (int)otype == OP_FC \
  || (int)otype == OP_FCNTL \
  || (int)otype == OP_FILENO \
  || (int)otype == OP_FLOCK \
  || (int)otype == OP_FORK \
  || (int)otype == OP_FORMLINE \
  || (int)otype == OP_FTATIME \
  || (int)otype == OP_FTBINARY \
  || (int)otype == OP_FTBLK \
  || (int)otype == OP_FTCHR \
  || (int)otype == OP_FTCTIME \
  || (int)otype == OP_FTDIR \
  || (int)otype == OP_FTEEXEC \
  || (int)otype == OP_FTEOWNED \
  || (int)otype == OP_FTEREAD \
  || (int)otype == OP_FTEWRITE \
  || (int)otype == OP_FTFILE \
  || (int)otype == OP_FTIS \
  || (int)otype == OP_FTLINK \
  || (int)otype == OP_FTMTIME \
  || (int)otype == OP_FTPIPE \
  || (int)otype == OP_FTREXEC \
  || (int)otype == OP_FTROWNED \
  || (int)otype == OP_FTRREAD \
  || (int)otype == OP_FTRWRITE \
  || (int)otype == OP_FTSGID \
  || (int)otype == OP_FTSIZE \
  || (int)otype == OP_FTSOCK \
  || (int)otype == OP_FTSUID \
  || (int)otype == OP_FTSVTX \
  || (int)otype == OP_FTTEXT \
  || (int)otype == OP_FTTTY \
  || (int)otype == OP_FTZERO \
  || (int)otype == OP_GE \
  || (int)otype == OP_GELEM \
  || (int)otype == OP_GETC \
  || (int)otype == OP_GETLOGIN \
  || (int)otype == OP_GETPEERNAME \
  || (int)otype == OP_GETPGRP \
  || (int)otype == OP_GETPPID \
  || (int)otype == OP_GETPRIORITY \
  || (int)otype == OP_GETSOCKNAME \
  || (int)otype == OP_GGRENT \
  || (int)otype == OP_GGRGID \
  || (int)otype == OP_GGRNAM \
  || (int)otype == OP_GHBYADDR \
  || (int)otype == OP_GHBYNAME \
  || (int)otype == OP_GHOSTENT \
  || (int)otype == OP_GLOB \
  || (int)otype == OP_GMTIME \
  || (int)otype == OP_GNBYADDR \
  || (int)otype == OP_GNBYNAME \
  || (int)otype == OP_GNETENT \
  || (int)otype == OP_GOTO \
  || (int)otype == OP_GPBYNAME \
  || (int)otype == OP_GPBYNUMBER \
  || (int)otype == OP_GPROTOENT \
  || (int)otype == OP_GPWENT \
  || (int)otype == OP_GPWNAM \
  || (int)otype == OP_GPWUID \
  || (int)otype == OP_GREPWHILE \
  || (int)otype == OP_GSBYNAME \
  || (int)otype == OP_GSBYPORT \
  || (int)otype == OP_GSERVENT \
  || (int)otype == OP_GSOCKOPT \
  || (int)otype == OP_GT \
  || (int)otype == OP_GV \
  || (int)otype == OP_GVSV \
  || (int)otype == OP_HELEM \
  || (int)otype == OP_HEX \
  || (int)otype == OP_HSLICE \
  || (int)otype == OP_INDEX \
  || (int)otype == OP_INT \
  || (int)otype == OP_IOCTL \
  || (int)otype == OP_I_ADD \
  || (int)otype == OP_I_DIVIDE \
  || (int)otype == OP_I_EQ \
  || (int)otype == OP_I_GE \
  || (int)otype == OP_I_GT \
  || (int)otype == OP_I_LE \
  || (int)otype == OP_I_LT \
  || (int)otype == OP_I_MODULO \
  || (int)otype == OP_I_MULTIPLY \
  || (int)otype == OP_I_NCMP \
  || (int)otype == OP_I_NE \
  || (int)otype == OP_I_NEGATE \
  || (int)otype == OP_I_POSTDEC \
  || (int)otype == OP_I_POSTINC \
  || (int)otype == OP_I_PREDEC \
  || (int)otype == OP_I_PREINC \
  || (int)otype == OP_I_SUBTRACT \
  || (int)otype == OP_JOIN \
  || (int)otype == OP_KEYS \
  || (int)otype == OP_KILL \
  || (int)otype == OP_LC \
  || (int)otype == OP_LCFIRST \
  || (int)otype == OP_LE \
  || (int)otype == OP_LEAVE \
  || (int)otype == OP_LEAVEGIVEN \
  || (int)otype == OP_LEAVELOOP \
  || (int)otype == OP_LEAVEWHEN \
  || (int)otype == OP_LEFT_SHIFT \
  || (int)otype == OP_LENGTH \
  || (int)otype == OP_LINK \
  || (int)otype == OP_LIST \
  || (int)otype == OP_LISTEN \
  || (int)otype == OP_LOCALTIME \
  || (int)otype == OP_LOCK \
  || (int)otype == OP_LOG \
  || (int)otype == OP_LSLICE \
  || (int)otype == OP_LSTAT \
  || (int)otype == OP_LT \
  || (int)otype == OP_MAPWHILE \
  || (int)otype == OP_MATCH \
  || (int)otype == OP_METHOD \
  || (int)otype == OP_METHOD_NAMED \
  || (int)otype == OP_MKDIR \
  || (int)otype == OP_MODULO \
  || (int)otype == OP_MSGCTL \
  || (int)otype == OP_MSGGET \
  || (int)otype == OP_MSGRCV \
  || (int)otype == OP_MSGSND \
  || (int)otype == OP_MULTIPLY \
  || (int)otype == OP_NCMP \
  || (int)otype == OP_NE \
  || (int)otype == OP_NEGATE \
  || (int)otype == OP_NOT \
  || (int)otype == OP_NULL \
  || (int)otype == OP_OCT \
  || (int)otype == OP_ONCE \
  || (int)otype == OP_OPEN \
  || (int)otype == OP_OPEN_DIR \
  || (int)otype == OP_OR \
  || (int)otype == OP_ORASSIGN \
  || (int)otype == OP_ORD \
  || (int)otype == OP_PACK \
  || (int)otype == OP_PADAV \
  || (int)otype == OP_PADHV \
  || (int)otype == OP_PADSV \
  || (int)otype == OP_PIPE_OP \
  || (int)otype == OP_POP \
  || (int)otype == OP_POS \
  || (int)otype == OP_POSTDEC \
  || (int)otype == OP_POSTINC \
  || (int)otype == OP_POW \
  || (int)otype == OP_PREDEC \
  || (int)otype == OP_PREINC \
  || (int)otype == OP_PRINT \
  || (int)otype == OP_PROTOTYPE \
  || (int)otype == OP_PRTF \
  || (int)otype == OP_PUSH \
  || (int)otype == OP_QUOTEMETA \
  || (int)otype == OP_RAND \
  || (int)otype == OP_REACH \
  || (int)otype == OP_READ \
  || (int)otype == OP_READDIR \
  || (int)otype == OP_READLINE \
  || (int)otype == OP_READLINK \
  || (int)otype == OP_RECV \
  || (int)otype == OP_REF \
  || (int)otype == OP_REFGEN \
  || (int)otype == OP_RENAME \
  || (int)otype == OP_REPEAT \
  || (int)otype == OP_REQUIRE \
  || (int)otype == OP_RESET \
  || (int)otype == OP_RETURN \
  || (int)otype == OP_REVERSE \
  || (int)otype == OP_REWINDDIR \
  || (int)otype == OP_RIGHT_SHIFT \
  || (int)otype == OP_RINDEX \
  || (int)otype == OP_RKEYS \
  || (int)otype == OP_RMDIR \
  || (int)otype == OP_RV2AV \
  || (int)otype == OP_RV2CV \
  || (int)otype == OP_RV2GV \
  || (int)otype == OP_RV2HV \
  || (int)otype == OP_RV2SV \
  || (int)otype == OP_RVALUES \
  || (int)otype == OP_SASSIGN \
  || (int)otype == OP_SAY \
  || (int)otype == OP_SCHOMP \
  || (int)otype == OP_SCHOP \
  || (int)otype == OP_SCMP \
  || (int)otype == OP_SCOPE \
  || (int)otype == OP_SEEK \
  || (int)otype == OP_SEEKDIR \
  || (int)otype == OP_SELECT \
  || (int)otype == OP_SEMCTL \
  || (int)otype == OP_SEMGET \
  || (int)otype == OP_SEMOP \
  || (int)otype == OP_SEND \
  || (int)otype == OP_SEQ \
  || (int)otype == OP_SETPGRP \
  || (int)otype == OP_SETPRIORITY \
  || (int)otype == OP_SGE \
  || (int)otype == OP_SGRENT \
  || (int)otype == OP_SGT \
  || (int)otype == OP_SHIFT \
  || (int)otype == OP_SHMCTL \
  || (int)otype == OP_SHMGET \
  || (int)otype == OP_SHMREAD \
  || (int)otype == OP_SHMWRITE \
  || (int)otype == OP_SHOSTENT \
  || (int)otype == OP_SHUTDOWN \
  || (int)otype == OP_SIN \
  || (int)otype == OP_SLE \
  || (int)otype == OP_SLEEP \
  || (int)otype == OP_SLT \
  || (int)otype == OP_SMARTMATCH \
  || (int)otype == OP_SNE \
  || (int)otype == OP_SNETENT \
  || (int)otype == OP_SOCKET \
  || (int)otype == OP_SOCKPAIR \
  || (int)otype == OP_SORT \
  || (int)otype == OP_SPLICE \
  || (int)otype == OP_SPLIT \
  || (int)otype == OP_SPRINTF \
  || (int)otype == OP_SPROTOENT \
  || (int)otype == OP_SPWENT \
  || (int)otype == OP_SQRT \
  || (int)otype == OP_SRAND \
  || (int)otype == OP_SREFGEN \
  || (int)otype == OP_SSELECT \
  || (int)otype == OP_SSERVENT \
  || (int)otype == OP_SSOCKOPT \
  || (int)otype == OP_STAT \
  || (int)otype == OP_STUB \
  || (int)otype == OP_STUDY \
  || (int)otype == OP_SUBST \
  || (int)otype == OP_SUBSTR \
  || (int)otype == OP_SUBTRACT \
  || (int)otype == OP_SYMLINK \
  || (int)otype == OP_SYSCALL \
  || (int)otype == OP_SYSOPEN \
  || (int)otype == OP_SYSREAD \
  || (int)otype == OP_SYSSEEK \
  || (int)otype == OP_SYSTEM \
  || (int)otype == OP_SYSWRITE \
  || (int)otype == OP_TELL \
  || (int)otype == OP_TELLDIR \
  || (int)otype == OP_TIED \
  || (int)otype == OP_TIME \
  || (int)otype == OP_TMS \
  || (int)otype == OP_TRUNCATE \
  || (int)otype == OP_UC \
  || (int)otype == OP_UCFIRST \
  || (int)otype == OP_UMASK \
  || (int)otype == OP_UNDEF \
  || (int)otype == OP_UNLINK \
  || (int)otype == OP_UNPACK \
  || (int)otype == OP_UNSHIFT \
  || (int)otype == OP_UNTIE \
  || (int)otype == OP_UTIME \
  || (int)otype == OP_VALUES \
  || (int)otype == OP_VEC \
  || (int)otype == OP_WAIT \
  || (int)otype == OP_WAITPID \
  || (int)otype == OP_WANTARRAY \
  || (int)otype == OP_WARN)



#endif
