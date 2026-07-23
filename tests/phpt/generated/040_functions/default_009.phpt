--TEST--
function default argument vector 009
--FILE--
<?php
function generated_default_009($left, $right = 29) {
    return $left + $right;
}
echo generated_default_009(52), ':', generated_default_009(52, 30);
--EXPECT--
81:82
