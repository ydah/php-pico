--TEST--
function default argument vector 022
--FILE--
<?php
function generated_default_022($left, $right = 68) {
    return $left + $right;
}
echo generated_default_022(117), ':', generated_default_022(117, 69);
--EXPECT--
185:186
