--TEST--
recursive function vector 002
--FILE--
<?php
function generated_recursive_002($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_002($value + $depth, $depth - 1);
}
echo generated_recursive_002(8, 3);
--EXPECT--
14
