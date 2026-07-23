--TEST--
function default argument vector 011
--FILE--
<?php
function generated_default_011($left, $right = 35) {
    return $left + $right;
}
echo generated_default_011(62), ':', generated_default_011(62, 36);
--EXPECT--
97:98
