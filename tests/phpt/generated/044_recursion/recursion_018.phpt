--TEST--
recursive function vector 018
--FILE--
<?php
function generated_recursive_018($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_018($value + $depth, $depth - 1);
}
echo generated_recursive_018(56, 5);
--EXPECT--
71
