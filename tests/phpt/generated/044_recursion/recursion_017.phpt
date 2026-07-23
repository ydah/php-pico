--TEST--
recursive function vector 017
--FILE--
<?php
function generated_recursive_017($value, $depth) {
    if ($depth === 0) { return $value; }
    return generated_recursive_017($value + $depth, $depth - 1);
}
echo generated_recursive_017(53, 4);
--EXPECT--
63
