--TEST--
function default argument vector 001
--FILE--
<?php
function generated_default_001($left, $right = 5) {
    return $left + $right;
}
echo generated_default_001(12), ':', generated_default_001(12, 6);
--EXPECT--
17:18
