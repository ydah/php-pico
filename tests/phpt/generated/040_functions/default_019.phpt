--TEST--
function default argument vector 019
--FILE--
<?php
function generated_default_019($left, $right = 59) {
    return $left + $right;
}
echo generated_default_019(102), ':', generated_default_019(102, 60);
--EXPECT--
161:162
