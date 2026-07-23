--TEST--
function default argument vector 002
--FILE--
<?php
function generated_default_002($left, $right = 8) {
    return $left + $right;
}
echo generated_default_002(17), ':', generated_default_002(17, 9);
--EXPECT--
25:26
