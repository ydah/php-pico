--TEST--
nested array copy-on-write vector 036
--FILE--
<?php
$tree = ['node' => ['value' => 145, 'items' => [146]]];
$copy = $tree;
$copy['node']['value'] += 3;
$copy['node']['items'][] = 150;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
145:148:146:146,150
