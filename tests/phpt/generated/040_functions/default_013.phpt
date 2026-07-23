--TEST--
function default argument vector 013
--FILE--
<?php
function generated_default_013($left, $right = 41) {
    return $left + $right;
}
echo generated_default_013(72), ':', generated_default_013(72, 42);
--EXPECT--
113:114
