--TEST--
function default argument vector 016
--FILE--
<?php
function generated_default_016($left, $right = 50) {
    return $left + $right;
}
echo generated_default_016(87), ':', generated_default_016(87, 51);
--EXPECT--
137:138
