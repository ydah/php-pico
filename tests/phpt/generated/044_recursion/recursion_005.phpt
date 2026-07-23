--TEST--
recursive function vector 005
--FILE--
<?php
function generated_recursive_005($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_005($value + $depth, $depth - 1);
}
echo generated_recursive_005(17, 6);
--EXPECT--
38
