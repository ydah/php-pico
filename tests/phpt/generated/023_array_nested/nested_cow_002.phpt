--TEST--
nested array copy-on-write vector 002
--FILE--
<?php
$tree = ['node' => ['value' => 9, 'items' => [10]]];
$copy = $tree;
$copy['node']['value'] += 4;
$copy['node']['items'][] = 15;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
9:13:10:10,15
