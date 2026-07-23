--TEST--
nested array copy-on-write vector 023
--FILE--
<?php
$tree = ['node' => ['value' => 93, 'items' => [94]]];
$copy = $tree;
$copy['node']['value'] += 4;
$copy['node']['items'][] = 99;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
93:97:94:94,99
