--TEST--
recursive function vector 013
--FILE--
<?php
function generated_recursive_013($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_013($value + $depth, $depth - 1);
}
echo generated_recursive_013(41, 7);
--EXPECT--
69
