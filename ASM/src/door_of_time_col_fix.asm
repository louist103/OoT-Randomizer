kill_door_of_time_col:
    move    gp, ra
    jal     0x80020EB4; Actor_Kill(&this->actor.child)
    move    a0, t8
    move    ra, gp
    lui     $at, 0x3F80 ; displaced instruction
    jr      ra
    mtc1    $at, $f6 ; displaced instruction