--TEST--
recursive function vector 015
--FILE--
<?php
function generated_recursive_015($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_015($value + $depth, $depth - 1);
}
echo generated_recursive_015(47, 2);
--EXPECT--
50
