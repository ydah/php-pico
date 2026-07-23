--TEST--
recursive function vector 008
--FILE--
<?php
function generated_recursive_008($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_008($value + $depth, $depth - 1);
}
echo generated_recursive_008(26, 2);
--EXPECT--
29
