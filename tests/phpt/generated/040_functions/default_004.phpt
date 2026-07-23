--TEST--
function default argument vector 004
--FILE--
<?php
function generated_default_004($left, $right = 14) {
    return $left + $right;
}
echo generated_default_004(27), ':', generated_default_004(27, 15);
--EXPECT--
41:42
