--TEST--
function default argument vector 024
--FILE--
<?php
function generated_default_024($left, $right = 74) {
    return $left + $right;
}
echo generated_default_024(127), ':', generated_default_024(127, 75);
--EXPECT--
201:202
