--TEST--
recursive function vector 012
--FILE--
<?php
function generated_recursive_012($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_012($value + $depth, $depth - 1);
}
echo generated_recursive_012(38, 6);
--EXPECT--
59
