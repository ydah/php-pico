--TEST--
recursive function vector 004
--FILE--
<?php
function generated_recursive_004($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_004($value + $depth, $depth - 1);
}
echo generated_recursive_004(14, 5);
--EXPECT--
29
