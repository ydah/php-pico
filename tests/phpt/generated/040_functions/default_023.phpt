--TEST--
function default argument vector 023
--FILE--
<?php
function generated_default_023($left, $right = 71) {
    return $left + $right;
}
echo generated_default_023(122), ':', generated_default_023(122, 72);
--EXPECT--
193:194
