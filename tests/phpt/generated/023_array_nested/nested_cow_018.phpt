--TEST--
nested array copy-on-write vector 018
--FILE--
<?php
$tree = ['node' => ['value' => 73, 'items' => [74]]];
$copy = $tree;
$copy['node']['value'] += 6;
$copy['node']['items'][] = 81;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
73:79:74:74,81
