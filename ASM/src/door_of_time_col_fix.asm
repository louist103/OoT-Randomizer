kill_door_of_time_col:
    addiu   sp, sp, -0x08
    sw      ra, 8(sp)
    jal     0x80020EB4; Actor_Kill(&this->actor.child)
    nop
    lw      ra, 8(sp)
    lui     $at, 0x3F80 ; displaced instruction
    addiu   sp, sp, 0x08
    jr      ra
    mtc1    $at, $f6 ; displaced instruction