--TEST--
recursive function vector 019
--FILE--
<?php
function generated_recursive_019($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_019($value + $depth, $depth - 1);
}
echo generated_recursive_019(59, 6);
--EXPECT--
80
