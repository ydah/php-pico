--TEST--
function default argument vector 014
--FILE--
<?php
function generated_default_014($left, $right = 44) {
    return $left + $right;
}
echo generated_default_014(77), ':', generated_default_014(77, 45);
--EXPECT--
121:122
