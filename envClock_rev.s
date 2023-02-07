VERSION = 1
REVISION = 1

.macro DATE
.ascii "22.7.2014"
.endm

.macro VERS
.ascii "envClock 1.1"
.endm

.macro VSTRING
.ascii "envClock 1.1 (22.7.2014)"
.byte 13,10,0
.endm

.macro VERSTAG
.byte 0
.ascii "$VER: envClock 1.1 (22.7.2014)"
.byte 0
.endm
