--TEST--
recursive function vector 001
--FILE--
<?php
function generated_recursive_001($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_001($value + $depth, $depth - 1);
}
echo generated_recursive_001(5, 2);
--EXPECT--
8
