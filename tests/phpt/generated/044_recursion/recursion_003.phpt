--TEST--
recursive function vector 003
--FILE--
<?php
function generated_recursive_003($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_003($value + $depth, $depth - 1);
}
echo generated_recursive_003(11, 4);
--EXPECT--
21
