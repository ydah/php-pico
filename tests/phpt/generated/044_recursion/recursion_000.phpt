--TEST--
recursive function vector 000
--FILE--
<?php
function generated_recursive_000($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_000($value + $depth, $depth - 1);
}
echo generated_recursive_000(2, 1);
--EXPECT--
3
