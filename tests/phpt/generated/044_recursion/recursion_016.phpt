--TEST--
recursive function vector 016
--FILE--
<?php
function generated_recursive_016($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_016($value + $depth, $depth - 1);
}
echo generated_recursive_016(50, 3);
--EXPECT--
56
