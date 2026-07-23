--TEST--
recursive function vector 007
--FILE--
<?php
function generated_recursive_007($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_007($value + $depth, $depth - 1);
}
echo generated_recursive_007(23, 1);
--EXPECT--
24
