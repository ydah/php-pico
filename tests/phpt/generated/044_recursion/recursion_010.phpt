--TEST--
recursive function vector 010
--FILE--
<?php
function generated_recursive_010($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_010($value + $depth, $depth - 1);
}
echo generated_recursive_010(32, 4);
--EXPECT--
42
