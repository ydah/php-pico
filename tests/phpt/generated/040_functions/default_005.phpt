--TEST--
function default argument vector 005
--FILE--
<?php
function generated_default_005($left, $right = 17) {
    return $left + $right;
}
echo generated_default_005(32), ':', generated_default_005(32, 18);
--EXPECT--
49:50
