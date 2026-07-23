--TEST--
recursive function vector 009
--FILE--
<?php
function generated_recursive_009($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_009($value + $depth, $depth - 1);
}
echo generated_recursive_009(29, 3);
--EXPECT--
35
