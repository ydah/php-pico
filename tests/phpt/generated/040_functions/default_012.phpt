--TEST--
function default argument vector 012
--FILE--
<?php
function generated_default_012($left, $right = 38) {
    return $left + $right;
}
echo generated_default_012(67), ':', generated_default_012(67, 39);
--EXPECT--
105:106
