--TEST--
function default argument vector 000
--FILE--
<?php
function generated_default_000($left, $right = 2) {
    return $left + $right;
}
echo generated_default_000(7), ':', generated_default_000(7, 3);
--EXPECT--
9:10
