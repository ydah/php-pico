--TEST--
function default argument vector 008
--FILE--
<?php
function generated_default_008($left, $right = 26) {
    return $left + $right;
}
echo generated_default_008(47), ':', generated_default_008(47, 27);
--EXPECT--
73:74
