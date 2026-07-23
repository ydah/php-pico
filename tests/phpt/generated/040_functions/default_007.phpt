--TEST--
function default argument vector 007
--FILE--
<?php
function generated_default_007($left, $right = 23) {
    return $left + $right;
}
echo generated_default_007(42), ':', generated_default_007(42, 24);
--EXPECT--
65:66
