--TEST--
private and protected member access vector 011
--FILE--
<?php
class GeneratedVisibility011 {
    private $left = 81; protected $right = 61;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility011())->total();
--EXPECT--
142
