--TEST--
function default argument vector 010
--FILE--
<?php
function generated_default_010($left, $right = 32) {
    return $left + $right;
}
echo generated_default_010(57), ':', generated_default_010(57, 33);
--EXPECT--
89:90
