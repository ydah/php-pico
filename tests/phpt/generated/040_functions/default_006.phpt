--TEST--
function default argument vector 006
--FILE--
<?php
function generated_default_006($left, $right = 20) {
    return $left + $right;
}
echo generated_default_006(37), ':', generated_default_006(37, 21);
--EXPECT--
57:58
