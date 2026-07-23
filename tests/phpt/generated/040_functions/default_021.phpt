--TEST--
function default argument vector 021
--FILE--
<?php
function generated_default_021($left, $right = 65) {
    return $left + $right;
}
echo generated_default_021(112), ':', generated_default_021(112, 66);
--EXPECT--
177:178
