--TEST--
function default argument vector 018
--FILE--
<?php
function generated_default_018($left, $right = 56) {
    return $left + $right;
}
echo generated_default_018(97), ':', generated_default_018(97, 57);
--EXPECT--
153:154
