--TEST--
recursive function vector 006
--FILE--
<?php
function generated_recursive_006($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_006($value + $depth, $depth - 1);
}
echo generated_recursive_006(20, 7);
--EXPECT--
48
