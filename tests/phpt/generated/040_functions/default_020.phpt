--TEST--
function default argument vector 020
--FILE--
<?php
function generated_default_020($left, $right = 62) {
    return $left + $right;
}
echo generated_default_020(107), ':', generated_default_020(107, 63);
--EXPECT--
169:170
