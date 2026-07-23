--TEST--
function default argument vector 003
--FILE--
<?php
function generated_default_003($left, $right = 11) {
    return $left + $right;
}
echo generated_default_003(22), ':', generated_default_003(22, 12);
--EXPECT--
33:34
