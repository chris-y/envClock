VERSION = 1
REVISION = 2

.macro DATE
.ascii "15.5.2024"
.endm

.macro VERS
.ascii "envClock 1.2"
.endm

.macro VSTRING
.ascii "envClock 1.2 (15.5.2024)"
.byte 13,10,0
.endm

.macro VERSTAG
.byte 0
.ascii "$VER: envClock 1.2 (15.5.2024)"
.byte 0
.endm
