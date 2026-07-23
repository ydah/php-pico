--TEST--
object clone and property COW vector 005
--FILE--
<?php
class GeneratedClone005 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone005([8, 15]);
$copy = clone $original;
$copy->items[0] = 53;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
8,15:53,15
