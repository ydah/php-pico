--TEST--
recursive function vector 011
--FILE--
<?php
function generated_recursive_011($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_011($value + $depth, $depth - 1);
}
echo generated_recursive_011(35, 5);
--EXPECT--
50
