--TEST--
recursive function vector 014
--FILE--
<?php
function generated_recursive_014($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_014($value + $depth, $depth - 1);
}
echo generated_recursive_014(44, 1);
--EXPECT--
45
